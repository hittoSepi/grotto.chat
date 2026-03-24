package fi.grotto.android.data.remote

import com.google.protobuf.ByteString
import fi.grotto.android.data.local.entity.MessageEntity
import fi.grotto.android.data.local.preferences.UserPreferences
import fi.grotto.android.data.remote.ConnectionState
import fi.grotto.android.data.repository.ChannelMemberRepository
import fi.grotto.android.data.repository.KeyRepository
import fi.grotto.android.data.repository.MessageRepository
import fi.grotto.android.crypto.NativeCrypto
import fi.grotto.android.crypto.NativeStore
import grotto.Grotto.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import timber.log.Timber
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicLong
import javax.inject.Inject
import javax.inject.Singleton

enum class AuthState { NONE, HELLO_SENT, CHALLENGE_RECEIVED, AUTH_SENT, AUTHENTICATED }

/**
 * Manages the full connection lifecycle:
 *   connect → HELLO → AUTH_CHALLENGE → AUTH_RESPONSE → AUTH_OK → KEY_UPLOAD
 *   then: send/receive encrypted chat messages, handle key exchange, ping/pong.
 */
@Singleton
class GrottoConnectionManager @Inject constructor(
    private val socket: GrottoSocket,
    private val userPreferences: UserPreferences,
    private val messageRepository: MessageRepository,
    private val keyRepository: KeyRepository,
    private val channelMemberRepository: ChannelMemberRepository,
    private val nativeStore: NativeStore,
) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val seq = AtomicLong(1)

    private val _authState = MutableStateFlow(AuthState.NONE)
    val authState: StateFlow<AuthState> = _authState.asStateFlow()

    private var userId: String = ""
    private var cryptoInitialized = false

    // Pending DMs waiting for key bundles
    private val pendingSends = ConcurrentHashMap<String, String>()
    
    // Callback for command responses (e.g., channel join)
    var onCommandResponse: ((success: Boolean, message: String, command: String) -> Unit)? = null

    // Callback for nick changes
    var onNickChange: ((oldNick: String, newNick: String) -> Unit)? = null

    // Callback for server errors (shown as snackbar/toast)
    var onServerError: ((code: Int, message: String) -> Unit)? = null

    // Callback for MOTD
    var onMotd: ((lines: List<String>) -> Unit)? = null

    // Callback for user info (WHOIS response)
    var onUserInfo: ((userId: String, nickname: String, isOnline: Boolean, channels: List<String>, fingerprint: String) -> Unit)? = null

    // Callback for presence updates
    var onPresenceUpdate: ((userId: String, status: String) -> Unit)? = null

    // Callback for join/part events
    var onUserJoined: ((channelId: String, userId: String, nickname: String) -> Unit)? = null
    var onUserLeft: ((channelId: String, userId: String, nickname: String) -> Unit)? = null

    // Callbacks for file transfer events
    var onFileChunk: ((grotto.Grotto.FileChunk) -> Unit)? = null
    var onFileProgress: ((grotto.Grotto.FileProgress) -> Unit)? = null
    var onFileComplete: ((grotto.Grotto.FileComplete) -> Unit)? = null
    var onFileError: ((grotto.Grotto.FileError) -> Unit)? = null

    // Tracked online users
    private val _onlineUsers = MutableStateFlow<Set<String>>(emptySet())
    val onlineUsers: StateFlow<Set<String>> = _onlineUsers.asStateFlow()

    /**
     * Connect to the server, authenticate, and start the message loop.
     * Call once from ChatViewModel or a Service.
     */
    fun start() {
        scope.launch {
            val address = userPreferences.serverAddress.first() ?: return@launch
            val port = userPreferences.port.first()
            val useTls = userPreferences.useTls.first()
            userId = userPreferences.nickname.first() ?: return@launch

            // Initialize native crypto engine
            if (!cryptoInitialized) {
                val ok = withContext(Dispatchers.Default) {
                    NativeCrypto.nativeInit(nativeStore, userId, "")
                }
                if (!ok) {
                    Timber.e("Failed to initialize NativeCrypto")
                    return@launch
                }
                cryptoInitialized = true
            }

            // Start listening for incoming frames
            launch { collectFrames() }

            // Connect socket (TLS or plaintext based on port/settings)
            Timber.i("Connecting to $address:$port (TLS=$useTls)")
            socket.connect(address, port, useTls)

            // Wait for connection, then send HELLO
            socket.connectionState.collect { state ->
                if (state == ConnectionState.CONNECTED && _authState.value == AuthState.NONE) {
                    sendHello()
                }
            }
        }
    }

    // ========================================================================
    // Outgoing: Auth flow
    // ========================================================================

    private fun sendHello() {
        val hello = Hello.newBuilder()
            .setProtocolVersion(1)
            .setClientVersion("grotto-android/0.15.0")
            .build()
        sendEnvelope(MessageType.MT_HELLO, hello.toByteArray())
        _authState.value = AuthState.HELLO_SENT
        Timber.i("Sent HELLO")
    }

    private fun handleAuthChallenge(payload: ByteArray) {
        _authState.value = AuthState.CHALLENGE_RECEIVED
        val challenge = AuthChallenge.parseFrom(payload)
        val nonce = challenge.nonce.toByteArray()
        Timber.i("Received AUTH_CHALLENGE (${nonce.size} bytes)")

        // Sign: nonce || user_id
        val signature = NativeCrypto.signChallenge(nonce)
        val identityPub = NativeCrypto.identityPub()
        val spk = NativeCrypto.currentSpk()

        if (signature == null || identityPub == null) {
            Timber.e("Failed to sign challenge")
            return
        }

        val authResp = AuthResponse.newBuilder()
            .setUserId(userId)
            .setIdentityPub(ByteString.copyFrom(identityPub))
            .setSignature(ByteString.copyFrom(signature))

        if (spk != null) {
            authResp.setSignedPrekey(ByteString.copyFrom(spk.pub))
            authResp.setSpkSig(ByteString.copyFrom(spk.sig))
        }

        sendEnvelope(MessageType.MT_AUTH_RESPONSE, authResp.build().toByteArray())
        _authState.value = AuthState.AUTH_SENT
        Timber.i("Sent AUTH_RESPONSE for '$userId'")
    }

    private fun handleAuthOk() {
        _authState.value = AuthState.AUTHENTICATED
        Timber.i("Authenticated as '$userId'")

        // Upload pre-keys
        scope.launch(Dispatchers.Default) {
            val uploadBytes = NativeCrypto.prepareRegistration(100) ?: return@launch
            val upload = parseKeyUploadBytes(uploadBytes)
            sendEnvelope(MessageType.MT_KEY_UPLOAD, upload.toByteArray())
            Timber.i("Sent KEY_UPLOAD (100 OPKs)")
        }
    }

    private fun handleAuthFail(payload: ByteArray) {
        val err = Error.parseFrom(payload)
        Timber.e("AUTH_FAIL: [${err.code}] ${err.message}")
        _authState.value = AuthState.NONE
    }

    // ========================================================================
    // Outgoing: Chat
    // ========================================================================

    /**
     * Send a chat message (encrypted) to a recipient or channel.
     * Called from ChatViewModel.
     */
    fun sendChat(recipientId: String, plaintext: String) {
        if (socket.connectionState.value != ConnectionState.CONNECTED) {
            Timber.w("Cannot send: socket not connected (state=${socket.connectionState.value})")
            return
        }
        if (_authState.value != AuthState.AUTHENTICATED) {
            Timber.w("Cannot send: not authenticated (state=${_authState.value})")
            return
        }

        scope.launch(Dispatchers.Default) {
            val isChannel = recipientId.startsWith("#")

            if (isChannel) {
                sendChannelMessage(recipientId, plaintext)
            } else {
                sendDmMessage(recipientId, plaintext)
            }
        }
    }
    
    /**
     * Send an IRC-style command to the server.
     * @param command Command name (e.g., "join", "part")
     * @param args Command arguments
     */
    fun sendCommand(command: String, vararg args: String): Boolean {
        if (socket.connectionState.value != ConnectionState.CONNECTED) {
            Timber.w("Cannot send command: socket not connected")
            return false
        }
        if (_authState.value != AuthState.AUTHENTICATED) {
            Timber.w("Cannot send command: not authenticated")
            return false
        }
        
        scope.launch(Dispatchers.Default) {
            val cmd = IrcCommand.newBuilder()
                .setCommand(command)
                .addAllArgs(args.toList())
                .build()
            sendEnvelope(MessageType.MT_COMMAND, cmd.toByteArray())
            Timber.d("Sent command: $command ${args.joinToString(" ")}")
        }
        return true
    }

    private fun sendDmMessage(recipientId: String, plaintext: String) {
        val ciphertextBytes = NativeCrypto.encrypt(recipientId, plaintext.toByteArray(Charsets.UTF_8))

        if (ciphertextBytes == null) {
            // No session yet — request key bundle, queue plaintext
            Timber.i("No session for '$recipientId', requesting key bundle")
            pendingSends[recipientId] = plaintext
            val kr = KeyRequest.newBuilder().setUserId(recipientId).build()
            sendEnvelope(MessageType.MT_KEY_REQUEST, kr.toByteArray())
            return
        }

        // Build ChatEnvelope with sender info
        // ciphertext_type: 1 = WHISPER_MESSAGE, 3 = PRE_KEY_MESSAGE
        val chat = createDmEnvelope(
            senderId = userId,
            recipientId = recipientId,
            ciphertext = ciphertextBytes,
            ciphertextType = 1 // WHISPER_MESSAGE - assumes session exists
        )
        sendEnvelope(MessageType.MT_CHAT_ENVELOPE, chat.toByteArray())
        Timber.d("Sent DM to '$recipientId'")
    }

    private fun sendChannelMessage(channelId: String, plaintext: String) {
        Timber.d("Sending channel message to '$channelId' as '$userId': $plaintext")
        
        // Encrypt - this creates session automatically on first call and returns SKDM
        val encryptResult = NativeCrypto.encryptGroup(channelId, plaintext.toByteArray(Charsets.UTF_8))
        
        if (encryptResult == null) {
            Timber.e("Failed to encrypt group message for $channelId")
            return
        }
        
        val ciphertextBytes = encryptResult.ciphertext
        val skdmBytes = encryptResult.skdm
        
        Timber.d("Encrypted message: ${ciphertextBytes.size} bytes, SKDM: ${skdmBytes?.size ?: 0} bytes")

        // Build ChatEnvelope with sender info and SKDM (if first message)
        val chat = createGroupEnvelope(
            senderId = userId,
            channelId = channelId,
            ciphertext = ciphertextBytes,
            skdm = skdmBytes  // Include SKDM for first message so receivers can decrypt
        )
        
        val chatBytes = chat.toByteArray()
        Timber.d("ChatEnvelope: ${chatBytes.size} bytes, sender='${chat.senderId}', recipient='${chat.recipientId}', type=${chat.ciphertextType}")
        
        val sent = sendEnvelope(MessageType.MT_CHAT_ENVELOPE, chatBytes)
        if (sent) {
            Timber.i("Sent channel message to '$channelId' (${chatBytes.size} bytes)")
        } else {
            Timber.e("Failed to send channel message to '$channelId'")
        }
    }

    // ========================================================================
    // Incoming: Frame dispatch
    // ========================================================================

    private suspend fun collectFrames() {
        socket.incomingFrames.collect { payload ->
            try {
                val envelope = Envelope.parseFrom(payload)
                dispatch(envelope)
            } catch (e: Exception) {
                Timber.e(e, "Failed to parse incoming envelope")
            }
        }
    }

    private fun dispatch(env: Envelope) {
        val payload = env.payload.toByteArray()
        when (env.type) {
            MessageType.MT_AUTH_CHALLENGE -> handleAuthChallenge(payload)
            MessageType.MT_AUTH_OK        -> handleAuthOk()
            MessageType.MT_AUTH_FAIL      -> handleAuthFail(payload)
            MessageType.MT_CHAT_ENVELOPE  -> handleIncomingChat(env)
            MessageType.MT_KEY_BUNDLE     -> handleKeyBundle(payload)
            MessageType.MT_PRESENCE       -> handlePresence(payload)
            MessageType.MT_PING           -> handlePing(env)
            MessageType.MT_COMMAND_RESPONSE -> handleCommandResponse(payload)
            MessageType.MT_ERROR          -> handleError(payload)
            MessageType.MT_NICK_CHANGE    -> handleNickChange(payload)
            MessageType.MT_MOTD           -> handleMotd(payload)
            MessageType.MT_USER_INFO      -> handleUserInfo(payload)
            MessageType.MT_FILE_CHUNK     -> handleFileChunk(payload)
            MessageType.MT_FILE_PROGRESS  -> handleFileProgress(payload)
            MessageType.MT_FILE_COMPLETE  -> handleFileComplete(payload)
            MessageType.MT_FILE_ERROR     -> handleFileError(payload)
            else -> Timber.d("Unhandled message type: ${env.type}")
        }
    }

    // ========================================================================
    // Incoming: Chat
    // ========================================================================

    private fun handleIncomingChat(env: Envelope) {
        scope.launch(Dispatchers.Default) {
            try {
                val chat = ChatEnvelope.parseFrom(env.payload)
                val senderId = chat.senderId
                val recipientId = chat.recipientId
                val ciphertext = chat.ciphertext.toByteArray()
                val type = chat.ciphertextType
                val skdm = chat.skdm.takeIf { it.size() > 0 }?.toByteArray()
                
                Timber.d("Received chat from '$senderId' to '$recipientId', type=$type, ${ciphertext.size} bytes")

                val plaintext: ByteArray? = if (recipientId.startsWith("#")) {
                    decryptIncomingGroupMessage(
                        senderId = senderId,
                        recipientId = recipientId,
                        ciphertext = ciphertext,
                        skdm = skdm,
                    )
                } else {
                    NativeCrypto.decrypt(senderId, recipientId, ciphertext, type.toInt())
                }

                if (plaintext == null) {
                    Timber.w("Failed to decrypt message from $senderId to $recipientId (type=$type, skdm=${skdm?.size ?: 0})")
                    if (recipientId.startsWith("#")) {
                        persistGroupDecryptFailure(senderId, recipientId, env.timestampMs)
                    }
                    return@launch
                }

                val text = String(plaintext, Charsets.UTF_8)
                Timber.i("Received message from '$senderId': $text")

                // Determine channel: for DMs use the sender's ID, for channels use recipient
                val channelId = if (recipientId.startsWith("#")) {
                    recipientId.removePrefix("#")
                } else {
                    senderId
                }

                // Persist to local DB
                val entity = MessageEntity(
                    channelId = channelId,
                    senderId = senderId,
                    content = text,
                    timestamp = if (env.timestampMs > 0) env.timestampMs else System.currentTimeMillis(),
                )
                messageRepository.insert(entity)

            } catch (e: Exception) {
                Timber.e(e, "Error handling incoming chat")
            }
        }
    }

    private suspend fun decryptIncomingGroupMessage(
        senderId: String,
        recipientId: String,
        ciphertext: ByteArray,
        skdm: ByteArray?,
    ): ByteArray? {
        val candidateChannelIds = buildList {
            add(recipientId)
            if (recipientId.startsWith("#")) {
                add(recipientId.removePrefix("#"))
            } else {
                add("#$recipientId")
            }
        }.distinct()

        for (channelId in candidateChannelIds) {
            val plaintext = NativeCrypto.decryptGroup(senderId, channelId, ciphertext, skdm)
            if (plaintext != null) {
                if (channelId != recipientId) {
                    Timber.i("Recovered group decrypt for $senderId using fallback channel id '$channelId' (original '$recipientId')")
                }
                return plaintext
            }
        }

        return null
    }

    private suspend fun persistGroupDecryptFailure(
        senderId: String,
        recipientId: String,
        timestampMs: Long,
    ) {
        val channelId = recipientId.removePrefix("#")
        val entity = MessageEntity(
            channelId = channelId,
            senderId = "system",
            content = "Could not decrypt a message from $senderId in $recipientId. Sender key may be missing or stale.",
            timestamp = if (timestampMs > 0) timestampMs else System.currentTimeMillis(),
            msgType = "system",
        )
        messageRepository.insert(entity)
    }

    // ========================================================================
    // Incoming: Key exchange
    // ========================================================================

    private fun handleKeyBundle(payload: ByteArray) {
        scope.launch(Dispatchers.Default) {
            try {
                val bundle = KeyBundle.parseFrom(payload)
                val recipientId = bundle.recipientFor
                if (recipientId.isNullOrEmpty()) {
                    Timber.w("KeyBundle missing recipient_for")
                    return@launch
                }

                // Establish X3DH session
                NativeCrypto.onKeyBundle(recipientId, bundle.toNativeBytes())
                Timber.i("Established session with '$recipientId'")

                // Flush pending plaintext
                val pending = pendingSends.remove(recipientId) ?: return@launch
                sendDmMessage(recipientId, pending)
                Timber.i("Flushed pending DM to '$recipientId'")
            } catch (e: Exception) {
                Timber.e(e, "Error handling key bundle")
            }
        }
    }

    // ========================================================================
    // Incoming: Misc
    // ========================================================================

    private fun handlePresence(payload: ByteArray) {
        scope.launch {
            try {
                val update = PresenceUpdate.parseFrom(payload)
                Timber.d("Presence: ${update.userId} -> ${update.status}")
                val statusName = update.status.name.lowercase()
                
                // Update in-memory online users set
                _onlineUsers.value = if (statusName == "online" || statusName == "away") {
                    _onlineUsers.value + update.userId
                } else {
                    _onlineUsers.value - update.userId
                }
                
                // Persist to database
                keyRepository.updatePresence(update.userId, statusName)
                
                onPresenceUpdate?.invoke(update.userId, statusName)
            } catch (e: Exception) {
                Timber.e(e, "Error parsing presence")
            }
        }
    }

    private fun handlePing(env: Envelope) {
        sendEnvelope(MessageType.MT_PONG, env.payload.toByteArray())
    }

    private fun handleError(payload: ByteArray) {
        try {
            val err = Error.parseFrom(payload)
            Timber.e("Server error: [${err.code}] ${err.message}")
            onServerError?.invoke(err.code, err.message)
        } catch (e: Exception) {
            Timber.e(e, "Error parsing error message")
        }
    }

    private fun handleNickChange(payload: ByteArray) {
        try {
            val nc = NickChange.parseFrom(payload)
            Timber.i("Nick change: ${nc.oldNick} -> ${nc.newNick}")
            onNickChange?.invoke(nc.oldNick, nc.newNick)
        } catch (e: Exception) {
            Timber.e(e, "Error parsing nick change")
        }
    }

    private fun handleMotd(payload: ByteArray) {
        try {
            val motd = MotdMessage.parseFrom(payload)
            Timber.i("MOTD received (${motd.linesList.size} lines)")
            onMotd?.invoke(motd.linesList)
        } catch (e: Exception) {
            Timber.e(e, "Error parsing MOTD")
        }
    }

    private fun handleUserInfo(payload: ByteArray) {
        try {
            val info = UserInfo.parseFrom(payload)
            Timber.d("User info: ${info.userId} (${info.nickname}), online=${info.isOnline}")
            onUserInfo?.invoke(
                info.userId,
                info.nickname,
                info.isOnline,
                info.channelsList,
                info.identityFingerprint
            )
        } catch (e: Exception) {
            Timber.e(e, "Error parsing user info")
        }
    }
    
    private fun handleCommandResponse(payload: ByteArray) {
        try {
            val response = CommandResponse.parseFrom(payload)
            Timber.d("Command response: ${response.command} - success=${response.success}, message=${response.message}")
            onCommandResponse?.invoke(response.success, response.message, response.command)
        } catch (e: Exception) {
            Timber.e(e, "Error parsing command response")
        }
    }

    // ========================================================================
    // File Transfer
    // ========================================================================

    fun sendFileUploadRequest(request: grotto.Grotto.FileUploadRequest): Boolean {
        return sendEnvelope(MessageType.MT_FILE_UPLOAD, request.toByteArray())
    }

    fun sendFileUploadChunk(chunk: grotto.Grotto.FileUploadChunk): Boolean {
        return sendEnvelope(MessageType.MT_FILE_UPLOAD, chunk.toByteArray())
    }

    fun sendFileDownloadRequest(request: grotto.Grotto.FileDownloadRequest): Boolean {
        return sendEnvelope(MessageType.MT_FILE_DOWNLOAD, request.toByteArray())
    }

    private fun handleFileChunk(payload: ByteArray) {
        try {
            val chunk = grotto.Grotto.FileChunk.parseFrom(payload)
            Timber.d("File chunk: ${chunk.fileId}, index=${chunk.chunkIndex}, size=${chunk.data.size()} bytes")
            onFileChunk?.invoke(chunk)
        } catch (e: Exception) {
            Timber.e(e, "Error parsing file chunk")
        }
    }

    private fun handleFileProgress(payload: ByteArray) {
        try {
            val progress = grotto.Grotto.FileProgress.parseFrom(payload)
            Timber.d("File progress: ${progress.fileId}, ${progress.percentComplete}%")
            onFileProgress?.invoke(progress)
        } catch (e: Exception) {
            Timber.e(e, "Error parsing file progress")
        }
    }

    private fun handleFileComplete(payload: ByteArray) {
        try {
            val complete = grotto.Grotto.FileComplete.parseFrom(payload)
            Timber.d("File complete: ${complete.fileId}, ${complete.totalBytes} bytes")
            onFileComplete?.invoke(complete)
        } catch (e: Exception) {
            Timber.e(e, "Error parsing file complete")
        }
    }

    private fun handleFileError(payload: ByteArray) {
        try {
            val error = grotto.Grotto.FileError.parseFrom(payload)
            Timber.e("File error: ${error.fileId}, code=${error.errorCode}, ${error.errorMessage}")
            onFileError?.invoke(error)
        } catch (e: Exception) {
            Timber.e(e, "Error parsing file error")
        }
    }

    // ========================================================================
    // Helpers
    // ===================================================================

    private fun sendEnvelope(type: MessageType, payload: ByteArray): Boolean {
        val env = Envelope.newBuilder()
            .setSeq(seq.getAndIncrement())
            .setTimestampMs(System.currentTimeMillis())
            .setType(type)
            .setPayload(ByteString.copyFrom(payload))
            .build()
        return socket.send(env.toByteArray())
    }
}
