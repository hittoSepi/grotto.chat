package fi.grotto.android.data.remote

import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import javax.inject.Inject

/**
 * Length-prefixed framing: 4-byte big-endian length + protobuf payload.
 * Max frame size: 64 KB.
 */
class FrameCodec @Inject constructor() {

    companion object {
        const val MAX_FRAME_SIZE = 65536
        const val HEADER_SIZE = 4
    }

    fun encode(payload: ByteArray): ByteArray {
        require(payload.size <= MAX_FRAME_SIZE) { "Frame too large: ${payload.size}" }
        val buffer = ByteBuffer.allocate(HEADER_SIZE + payload.size)
        buffer.order(ByteOrder.BIG_ENDIAN)
        buffer.putInt(payload.size)
        buffer.put(payload)
        return buffer.array()
    }

    /**
     * Decodes a frame from a ByteArray (for WebSocket use).
     * Returns null if the frame is invalid.
     */
    fun decode(frame: ByteArray): ByteArray? {
        if (frame.size < HEADER_SIZE) return null
        
        val buffer = ByteBuffer.wrap(frame)
        buffer.order(ByteOrder.BIG_ENDIAN)
        
        val length = buffer.int
        if (length < 0 || length > MAX_FRAME_SIZE) return null
        if (frame.size < HEADER_SIZE + length) return null
        
        val payload = ByteArray(length)
        buffer.get(payload)
        return payload
    }

    suspend fun decode(inputStream: InputStream): ByteArray? {
        // TODO: implement actual frame reading from TLS socket for raw TCP mode
        return null
    }
}
