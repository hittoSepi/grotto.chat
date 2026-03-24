package fi.grotto.android.ui.screen.chat

import android.net.Uri
import androidx.lifecycle.SavedStateHandle
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import fi.grotto.android.data.local.entity.ChannelRole
import fi.grotto.android.data.local.entity.MessageEntity
import fi.grotto.android.data.local.preferences.UserPreferences
import fi.grotto.android.data.remote.AuthState
import fi.grotto.android.data.remote.ConnectionState
import fi.grotto.android.data.remote.GrottoConnectionManager
import fi.grotto.android.data.remote.GrottoSocket
import fi.grotto.android.data.repository.ChannelMemberRepository
import fi.grotto.android.data.repository.ChannelRepository
import fi.grotto.android.data.repository.FileRepository
import fi.grotto.android.data.repository.LinkPreviewRepository
import fi.grotto.android.data.repository.MessageRepository
import fi.grotto.android.data.repository.VoiceRepository
import fi.grotto.android.domain.model.FileTransfer
import fi.grotto.android.domain.model.LinkPreview
import fi.grotto.android.domain.model.Message
import fi.grotto.android.domain.model.SendStatus
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import timber.log.Timber
import javax.inject.Inject

data class ChatUiState(
    val channelId: String = "general",
    val channelName: String = "#general",
    val topic: String? = null,
    val isEncrypted: Boolean = true,
    val isConnected: Boolean = false,
    val currentUserId: String = "",
    val currentUserRole: ChannelRole = ChannelRole.REGULAR,
    val messages: List<Message> = emptyList(),
    val inputText: String = "",
    val voiceActive: Boolean = false,
    val voiceChannelName: String? = null,
    val voiceParticipantCount: Int = 0,
    val screenCaptureEnabled: Boolean = false,
    val activeFileTransfers: List<FileTransfer> = emptyList(),
    val connectionError: String? = null,
    val isConnecting: Boolean = false,
    val isSearchActive: Boolean = false,
    val searchQuery: String = "",
    val searchResults: List<Message> = emptyList(),
    val linkPreviews: Map<String, LinkPreview> = emptyMap(),  // URL -> Preview cache
    val linkPreviewsEnabled: Boolean = true,
)

@HiltViewModel
class ChatViewModel @Inject constructor(
    savedStateHandle: SavedStateHandle,
    private val messageRepository: MessageRepository,
    private val channelRepository: ChannelRepository,
    private val channelMemberRepository: ChannelMemberRepository,
    private val voiceRepository: VoiceRepository,
    private val fileRepository: FileRepository,
    private val linkPreviewRepository: LinkPreviewRepository,
    private val userPreferences: UserPreferences,
    private val grottoSocket: GrottoSocket,
    private val connectionManager: GrottoConnectionManager,
) : ViewModel() {

    private val channelId: String = savedStateHandle["channelId"] ?: "general"

    private val _uiState = MutableStateFlow(
        ChatUiState(
            channelId = channelId,
            channelName = "#$channelId",
        )
    )
    val uiState: StateFlow<ChatUiState> = _uiState.asStateFlow()

    private var hasJoinedChannel = false

    init {
        // Load current user and start connection + auth
        viewModelScope.launch {
            val nickname = userPreferences.nickname.first() ?: ""
            _uiState.update { it.copy(currentUserId = nickname) }
            connectionManager.start()

            // Load topic from DB
            val channel = channelRepository.getChannelById(channelId)
            channel?.topic?.let { topic ->
                _uiState.update { it.copy(topic = topic) }
            }

            // Auto-join the channel when connected
            kotlinx.coroutines.delay(1000) // Wait for connection
            joinChannel("#$channelId")
        }

        // Listen for command responses (topic, names, etc.)
        connectionManager.onCommandResponse = { success, message, command ->
            if (success) {
                when (command) {
                    "topic" -> {
                        // Message format: "Topic for #channel: the topic text"
                        val topicMatch = Regex("Topic for #\\S+: (.+)").find(message)
                        val topic = topicMatch?.groupValues?.get(1) ?: message
                        _uiState.update { it.copy(topic = topic) }
                        viewModelScope.launch {
                            channelRepository.updateTopic(channelId, topic)
                        }
                    }
                    "names" -> {
                        // Parse member list from names response
                        viewModelScope.launch {
                            channelMemberRepository.syncMemberList(channelId, message)
                        }
                    }
                }
            }
        }

        // Listen for join/part events
        connectionManager.onUserJoined = { ch, userId, nickname ->
            viewModelScope.launch {
                if (ch.removePrefix("#") == channelId) {
                    channelMemberRepository.addMember(channelId, userId, nickname)
                }
            }
        }
        connectionManager.onUserLeft = { ch, userId, nickname ->
            viewModelScope.launch {
                if (ch.removePrefix("#") == channelId) {
                    channelMemberRepository.removeMember(channelId, userId)
                }
            }
        }

        // Observe screen capture setting
        userPreferences.screenCapture.onEach { enabled ->
            _uiState.update { it.copy(screenCaptureEnabled = enabled) }
        }.launchIn(viewModelScope)

        // Observe link preview setting
        userPreferences.linkPreviewsEnabled.onEach { enabled ->
            _uiState.update { it.copy(linkPreviewsEnabled = enabled) }
        }.launchIn(viewModelScope)

        // Combine repository flows to update UI
        combine(
            messageRepository.getMessages(channelId),
            grottoSocket.connectionState,
            voiceRepository.voiceState,
        ) { entities, connectionState, voiceState ->
            
            val messages = entities.map { it.toDomainModel() }
            
            _uiState.update { state ->
                state.copy(
                    messages = messages,
                    isConnected = connectionState == ConnectionState.CONNECTED,
                    isConnecting = connectionState == ConnectionState.CONNECTING,
                    voiceActive = voiceState.isInVoice && voiceState.channelId == channelId,
                    voiceChannelName = if (voiceState.isInVoice) "#$channelId" else null,
                    voiceParticipantCount = voiceState.participants.size,
                )
            }

            // Fetch link previews for new messages (if enabled)
            if (_uiState.value.linkPreviewsEnabled) {
                messages.forEach { message ->
                    extractUrl(message.content)?.let { url ->
                        fetchLinkPreview(url)
                    }
                }
            }
        }.launchIn(viewModelScope)
        
        // Observe auth state for errors
        connectionManager.authState.onEach { authState ->
            Timber.d("Auth state changed: $authState")
        }.launchIn(viewModelScope)
        
        // Observe file transfer updates
        viewModelScope.launch {
            fileRepository.transferUpdates.collect { transfer ->
                _uiState.update { state ->
                    val updatedList = state.activeFileTransfers.map { 
                        if (it.fileId == transfer.fileId) transfer else it 
                    }
                    state.copy(activeFileTransfers = updatedList)
                }
            }
        }
    }

    fun onInputChanged(text: String) = _uiState.update { it.copy(inputText = text) }

    /**
     * Manually trigger reconnection to the server.
     */
    fun reconnect() {
        viewModelScope.launch {
            Timber.i("Manual reconnect requested")
            _uiState.update { it.copy(connectionError = null) }
            connectionManager.start()
        }
    }
    
    /**
     * Dismiss connection error message.
     */
    fun dismissError() {
        _uiState.update { it.copy(connectionError = null) }
    }

    fun sendMessage() {
        val state = _uiState.value
        val text = state.inputText.trim()
        if (text.isEmpty()) return

        // Check if this is a command (starts with /)
        if (text.startsWith("/")) {
            handleCommand(text)
            return
        }

        // Check if we've joined the channel
        if (!hasJoinedChannel) {
            _uiState.update { it.copy(connectionError = "Not joined to channel. Use /join #${channelId} first") }
            return
        }

        val recipientId = "#$channelId" // channels prefixed with #

        viewModelScope.launch {
            // Save to local DB immediately for UI feedback
            val entity = MessageEntity(
                channelId = channelId,
                senderId = state.currentUserId.ifEmpty { "me" },
                content = text,
                timestamp = System.currentTimeMillis(),
                sendStatus = SendStatus.SENDING.name.lowercase(),
            )
            val id = messageRepository.insert(entity)
            _uiState.update { it.copy(inputText = "") }

            try {
                connectionManager.sendChat(recipientId, text)
                messageRepository.updateSendStatus(id, SendStatus.SENT.name.lowercase())
            } catch (e: Exception) {
                messageRepository.updateSendStatus(id, SendStatus.FAILED.name.lowercase())
            }
        }
    }
    
    fun joinChannel(channelName: String) {
        val name = if (channelName.startsWith("#")) channelName else "#$channelName"
        val accepted = connectionManager.sendCommand("join", name)
        if (accepted) {
            hasJoinedChannel = true
        } else {
            _uiState.update {
                it.copy(connectionError = "Cannot join channel before authentication completes")
            }
        }
    }
    
    private fun handleCommand(input: String) {
        val parts = input.substring(1).split(" ", limit = 2)
        if (parts.isEmpty()) return

        val command = parts[0].lowercase()
        val rawArgs = if (parts.size > 1) parts[1] else ""
        val args = rawArgs.split(" ").filter { it.isNotEmpty() }

        when (command) {
            "join" -> {
                if (args.isNotEmpty()) {
                    val channelName = args[0]
                    val accepted = connectionManager.sendCommand("join", channelName)
                    if (accepted) {
                        _uiState.update { it.copy(inputText = "") }
                    } else {
                        _uiState.update {
                            it.copy(connectionError = "Cannot join channel before authentication completes")
                        }
                    }
                }
            }
            "part", "leave" -> {
                val channelName = args.firstOrNull() ?: "#$channelId"
                connectionManager.sendCommand("part", channelName)
                _uiState.update { it.copy(inputText = "") }
            }
            "nick" -> {
                if (args.isNotEmpty()) {
                    connectionManager.sendCommand("nick", args[0])
                    _uiState.update { it.copy(inputText = "") }
                }
            }
            "me", "action" -> {
                if (rawArgs.isNotEmpty()) {
                    connectionManager.sendCommand("me", rawArgs)
                    _uiState.update { it.copy(inputText = "") }
                }
            }
            "msg", "query" -> {
                if (args.size >= 2) {
                    val recipient = args[0]
                    val message = rawArgs.substringAfter("$recipient ")
                    connectionManager.sendChat(recipient, message)
                    _uiState.update { it.copy(inputText = "") }
                }
            }
            "whois" -> {
                if (args.isNotEmpty()) {
                    connectionManager.sendCommand("whois", args[0])
                    _uiState.update { it.copy(inputText = "") }
                }
            }
            "topic" -> {
                if (rawArgs.isNotEmpty()) {
                    connectionManager.sendCommand("topic", "#$channelId", rawArgs)
                } else {
                    connectionManager.sendCommand("topic", "#$channelId")
                }
                _uiState.update { it.copy(inputText = "") }
            }
            "names" -> {
                connectionManager.sendCommand("names")
                _uiState.update { it.copy(inputText = "") }
            }
            "kick" -> {
                if (args.isNotEmpty()) {
                    val user = args[0]
                    val reason = if (args.size > 1) rawArgs.substringAfter("$user ") else ""
                    connectionManager.sendCommand("kick", "#$channelId", user, reason)
                    _uiState.update { it.copy(inputText = "") }
                }
            }
            "ban" -> {
                if (args.isNotEmpty()) {
                    val user = args[0]
                    val reason = if (args.size > 1) rawArgs.substringAfter("$user ") else ""
                    connectionManager.sendCommand("ban", "#$channelId", user, reason)
                    _uiState.update { it.copy(inputText = "") }
                }
            }
            "invite" -> {
                if (args.isNotEmpty()) {
                    connectionManager.sendCommand("invite", "#$channelId", args[0])
                    _uiState.update { it.copy(inputText = "") }
                }
            }
            "password", "pass" -> {
                if (args.isNotEmpty()) {
                    connectionManager.sendCommand("password", *args.toTypedArray())
                    _uiState.update { it.copy(inputText = "") }
                }
            }
            "quit" -> {
                val reason = rawArgs.ifEmpty { "Leaving" }
                connectionManager.sendCommand("quit", reason)
                _uiState.update { it.copy(inputText = "") }
            }
            else -> {
                // Forward unknown commands to server
                connectionManager.sendCommand(command, *args.toTypedArray())
                _uiState.update { it.copy(inputText = "") }
            }
        }
    }

    /**
     * Send a command to the server from UI actions (e.g., member list actions)
     */
    fun sendCommand(command: String, vararg args: String) {
        connectionManager.sendCommand(command, *args)
    }
    
    // ============================================================================
    // Search
    // ============================================================================

    fun toggleSearch() {
        _uiState.update {
            if (it.isSearchActive) {
                it.copy(isSearchActive = false, searchQuery = "", searchResults = emptyList())
            } else {
                it.copy(isSearchActive = true)
            }
        }
    }

    fun onSearchQueryChanged(query: String) {
        _uiState.update { it.copy(searchQuery = query) }
        if (query.length >= 2) {
            viewModelScope.launch {
                val results = messageRepository.searchMessages(channelId, query)
                _uiState.update { state ->
                    state.copy(searchResults = results.map { it.toDomainModel() })
                }
            }
        } else {
            _uiState.update { it.copy(searchResults = emptyList()) }
        }
    }

    fun retryMessage(messageId: Long) {
        viewModelScope.launch {
            val msg = messageRepository.getMessageById(messageId) ?: return@launch
            messageRepository.updateSendStatus(messageId, SendStatus.SENDING.name.lowercase())
            try {
                connectionManager.sendChat("#${msg.channelId}", msg.content)
                messageRepository.updateSendStatus(messageId, SendStatus.SENT.name.lowercase())
            } catch (e: Exception) {
                messageRepository.updateSendStatus(messageId, SendStatus.FAILED.name.lowercase())
            }
        }
    }

    fun deleteMessage(messageId: Long) {
        viewModelScope.launch {
            messageRepository.deleteById(messageId)
        }
    }
    
    // ============================================================================
    // File Transfer
    // ============================================================================
    
    fun uploadFile(uri: Uri) {
        viewModelScope.launch {
            fileRepository.uploadFile(
                uri = uri,
                channelId = channelId
            ).onSuccess { transfer ->
                _uiState.update { state ->
                    state.copy(activeFileTransfers = state.activeFileTransfers + transfer)
                }
            }.onFailure { error ->
                // Error handling - could emit an event
            }
        }
    }
    
    fun cancelFileTransfer(fileId: String) {
        fileRepository.cancelTransfer(fileId)
    }
    
    fun clearCompletedTransfers() {
        fileRepository.clearCompletedTransfers()
        _uiState.update { state ->
            state.copy(activeFileTransfers = state.activeFileTransfers.filter { 
                it.status != fi.grotto.android.domain.model.TransferStatus.COMPLETED &&
                it.status != fi.grotto.android.domain.model.TransferStatus.FAILED &&
                it.status != fi.grotto.android.domain.model.TransferStatus.CANCELLED
            })
        }
    }

    private fun MessageEntity.toDomainModel(): Message {
        // Get cached link preview if available
        val cachedPreview = extractUrl(content)?.let { url ->
            _uiState.value.linkPreviews[url]
        }
        
        return Message(
            id = id,
            channelId = channelId,
            senderId = senderId,
            content = content,
            timestamp = timestamp,
            type = when (msgType) {
                "system" -> fi.grotto.android.domain.model.MessageType.SYSTEM
                "action" -> fi.grotto.android.domain.model.MessageType.ACTION
                else -> fi.grotto.android.domain.model.MessageType.CHAT
            },
            sendStatus = when (sendStatus) {
                "sending" -> SendStatus.SENDING
                "failed" -> SendStatus.FAILED
                else -> SendStatus.SENT
            },
            linkPreview = cachedPreview,
        )
    }
    
    private fun extractUrl(content: String): String? {
        val urlRegex = "(https?://[^\\s]+)".toRegex()
        val match = urlRegex.find(content) ?: return null
        return match.value.trimEnd(',', '.', '!', '?', ')')
    }

    private fun fetchLinkPreview(url: String) {
        // Skip if already cached or currently fetching
        if (_uiState.value.linkPreviews.containsKey(url)) return
        
        viewModelScope.launch {
            try {
                val preview = linkPreviewRepository.getLinkPreview(url)
                preview?.let {
                    _uiState.update { state ->
                        state.copy(
                            linkPreviews = state.linkPreviews + (url to it)
                        )
                    }
                }
            } catch (e: Exception) {
                Timber.e(e, "Failed to fetch link preview for $url")
            }
        }
    }
}
