package fi.grotto.android.data.remote

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import timber.log.Timber
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.IOException
import java.net.InetSocketAddress
import java.net.Socket
import javax.inject.Inject
import javax.inject.Singleton

enum class ConnectionState { DISCONNECTED, CONNECTING, CONNECTED }

/**
 * Raw TCP socket with optional TLS and 4-byte big-endian length-prefixed framing.
 * Matches the server's Boost.Asio TLS/plaintext + length-prefix wire protocol.
 */
@Singleton
class GrottoSocket @Inject constructor(
    private val frameCodec: FrameCodec,
    private val reconnectPolicy: ReconnectPolicy,
) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()

    private val _incomingFrames = MutableSharedFlow<ByteArray>(extraBufferCapacity = 64)
    val incomingFrames: SharedFlow<ByteArray> = _incomingFrames.asSharedFlow()

    private var socket: Socket? = null
    private var output: DataOutputStream? = null
    private val writeLock = Any()

    private var currentHost: String? = null
    private var currentPort: Int = 0
    private var useTls: Boolean = true
    private var shouldReconnect = true

    /**
     * Connect to server with automatic TLS detection based on port.
     * Port 6667 = plaintext, Port 6697 = TLS, others = TLS by default
     */
    suspend fun connect(host: String, port: Int, tls: Boolean? = null) {
        if (_connectionState.value != ConnectionState.DISCONNECTED) {
            Timber.w("Already connecting or connected, ignoring connect request")
            return
        }

        currentHost = host
        currentPort = port
        // Auto-detect TLS: 6667 = plaintext, 6697 = TLS, others = use provided or default to TLS
        useTls = tls ?: when (port) {
            6667 -> false
            6697 -> true
            else -> true
        }
        shouldReconnect = true

        _connectionState.value = ConnectionState.CONNECTING
        Timber.i("Connecting to $host:$port (TLS=$useTls)...")

        withContext(Dispatchers.IO) {
            try {
                Timber.d("Starting TCP connection to $host:$port...")
                
                val connectedSocket = if (useTls) {
                    createTlsSocket(host, port)
                } else {
                    createPlainSocket(host, port)
                }
                
                socket = connectedSocket
                output = DataOutputStream(connectedSocket.getOutputStream())

                _connectionState.value = ConnectionState.CONNECTED
                reconnectPolicy.reset()
                Timber.i("Connected to $host:$port (TLS=$useTls)")

                // Start read loop
                scope.launch { readLoop(connectedSocket) }

            } catch (e: java.net.UnknownHostException) {
                Timber.e(e, "Unknown host: $host")
                handleDisconnect("Unknown host: ${e.message}")
            } catch (e: java.net.ConnectException) {
                Timber.e(e, "Connection refused to $host:$port")
                handleDisconnect("Connection refused: ${e.message}")
            } catch (e: javax.net.ssl.SSLException) {
                Timber.e(e, "SSL/TLS error connecting to $host:$port")
                handleDisconnect("SSL error: ${e.message}. Try port 6667 for plaintext.")
            } catch (e: Exception) {
                Timber.e(e, "Failed to connect to $host:$port: ${e.javaClass.simpleName}")
                handleDisconnect("Connection failed: ${e.message}")
            }
        }
    }

    private fun createTlsSocket(host: String, port: Int): javax.net.ssl.SSLSocket {
        val sslContext = javax.net.ssl.SSLContext.getInstance("TLS")
        sslContext.init(null, null, null)

        val sslSocket = sslContext.socketFactory.createSocket() as javax.net.ssl.SSLSocket
        // Allow TLS 1.2 and 1.3 for better compatibility
        sslSocket.enabledProtocols = arrayOf("TLSv1.3", "TLSv1.2")
        sslSocket.soTimeout = 0
        
        sslSocket.connect(InetSocketAddress(host, port), 30_000)
        
        Timber.d("Starting TLS handshake...")
        sslSocket.startHandshake()
        
        val session = sslSocket.session
        Timber.i("TLS handshake complete: ${session.protocol} with ${session.cipherSuite}")
        
        return sslSocket
    }

    private fun createPlainSocket(host: String, port: Int): Socket {
        val plainSocket = Socket()
        plainSocket.soTimeout = 0
        plainSocket.connect(InetSocketAddress(host, port), 30_000)
        Timber.i("Plaintext TCP connection established")
        return plainSocket
    }

    fun send(payload: ByteArray): Boolean {
        if (_connectionState.value != ConnectionState.CONNECTED) {
            Timber.w("Cannot send: not connected")
            return false
        }

        return try {
            val frame = frameCodec.encode(payload)
            synchronized(writeLock) {
                output?.write(frame)
                output?.flush()
            }
            true
        } catch (e: IOException) {
            Timber.e(e, "Error sending frame")
            handleDisconnect("Write failed: ${e.message}")
            false
        }
    }

    fun disconnect() {
        shouldReconnect = false
        reconnectPolicy.reset()
        closeSocket()
        _connectionState.value = ConnectionState.DISCONNECTED
    }

    private fun readLoop(socket: Socket) {
        val input = DataInputStream(socket.getInputStream())
        try {
            while (_connectionState.value == ConnectionState.CONNECTED) {
                // Read 4-byte length header (big-endian)
                val length = input.readInt()
                if (length < 0 || length > FrameCodec.MAX_FRAME_SIZE) {
                    Timber.w("Invalid frame length: $length")
                    break
                }

                // Read payload
                val payload = ByteArray(length)
                input.readFully(payload)

                _incomingFrames.tryEmit(payload)
            }
        } catch (e: IOException) {
            if (_connectionState.value == ConnectionState.CONNECTED) {
                Timber.e(e, "Read loop error")
            }
        } finally {
            handleDisconnect("Read loop ended")
        }
    }

    private fun handleDisconnect(reason: String) {
        if (_connectionState.value == ConnectionState.DISCONNECTED) return
        Timber.i("Disconnected: $reason")
        closeSocket()
        _connectionState.value = ConnectionState.DISCONNECTED
        if (shouldReconnect) {
            scheduleReconnect()
        }
    }

    private fun closeSocket() {
        try {
            output?.close()
        } catch (_: Exception) {}
        try {
            socket?.close()
        } catch (_: Exception) {}
        output = null
        socket = null
    }

    private fun scheduleReconnect() {
        if (!shouldReconnect || !reconnectPolicy.hasAttemptsLeft()) return

        scope.launch {
            val delayMs = reconnectPolicy.nextDelayMs()
            Timber.i("Reconnecting in ${delayMs}ms...")
            delay(delayMs)

            if (isActive && shouldReconnect && _connectionState.value == ConnectionState.DISCONNECTED) {
                currentHost?.let { host ->
                    connect(host, currentPort, useTls)
                }
            }
        }
    }
}
