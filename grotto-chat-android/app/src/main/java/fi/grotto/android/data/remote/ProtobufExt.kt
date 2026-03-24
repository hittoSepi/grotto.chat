package fi.grotto.android.data.remote

import com.google.protobuf.ByteString
import grotto.Grotto.ChatEnvelope
import grotto.Grotto.KeyBundle
import grotto.Grotto.KeyUpload
// Import Kotlin extension functions for protobuf
import grotto.copy
import grotto.keyBundle

/**
 * Extension functions for working with Protocol Buffers.
 */

// ============================================================================
// ChatEnvelope Helpers
// ============================================================================

/**
 * Create a ChatEnvelope for a direct message.
 */
fun createDmEnvelope(
    senderId: String,
    recipientId: String,
    ciphertext: ByteArray,
    ciphertextType: Int
): ChatEnvelope {
    return ChatEnvelope.newBuilder()
        .setSenderId(senderId)
        .setRecipientId(recipientId)
        .setCiphertext(ByteString.copyFrom(ciphertext))
        .setCiphertextType(ciphertextType)
        .build()
}

/**
 * Create a ChatEnvelope for a group message.
 */
fun createGroupEnvelope(
    senderId: String,
    channelId: String,
    ciphertext: ByteArray,
    skdm: ByteArray? = null
): ChatEnvelope {
    val builder = ChatEnvelope.newBuilder()
        .setSenderId(senderId)
        .setRecipientId(channelId)
        .setCiphertext(ByteString.copyFrom(ciphertext))
        .setCiphertextType(4) // SENDER_KEY_MESSAGE
    
    if (skdm != null) {
        builder.setSkdm(ByteString.copyFrom(skdm))
    }
    
    return builder.build()
}

// ============================================================================
// KeyBundle Helpers
// ============================================================================

/**
 * Serialize KeyBundle to bytes for native crypto processing.
 * 
 * Format matches the simplified format used by the native layer:
 * [4 bytes spk_id LE] [32 bytes spk_pub] [4 bytes sig_len LE] [sig bytes]
 * [4 bytes opk_count LE] for each: [4 bytes id LE] [32 bytes pub]
 */
fun KeyBundle.toNativeBytes(): ByteArray {
    val data = mutableListOf<Byte>()
    
    // Helper to append little-endian uint32
    fun appendU32(value: Int) {
        data.add((value and 0xFF).toByte())
        data.add(((value shr 8) and 0xFF).toByte())
        data.add(((value shr 16) and 0xFF).toByte())
        data.add(((value shr 24) and 0xFF).toByte())
    }
    
    // SPK ID (use Java-style getter for protobuf)
    appendU32(spkId)
    
    // SPK public key
    val spkPub = signedPrekey.toByteArray()
    spkPub.forEach { data.add(it) }
    
    // SPK signature
    val spkSig = spkSignature.toByteArray()
    appendU32(spkSig.size)
    spkSig.forEach { data.add(it) }
    
    // One-time prekey (if present)
    if (oneTimePrekey.size() > 0) {
        appendU32(1) // count = 1
        appendU32(opkId)
        oneTimePrekey.toByteArray().forEach { data.add(it) }
    } else {
        appendU32(0) // count = 0
    }
    
    return data.toByteArray()
}

// ============================================================================
// KeyUpload Helpers
// ============================================================================

/**
 * Parse KeyUpload bytes from native crypto.
 */
fun parseKeyUploadBytes(data: ByteArray): KeyUpload {
    val builder = KeyUpload.newBuilder()
    var offset = 0
    
    // Helper to read little-endian uint32
    fun readU32(): Int {
        val value = data[offset].toInt() and 0xFF or
                   ((data[offset + 1].toInt() and 0xFF) shl 8) or
                   ((data[offset + 2].toInt() and 0xFF) shl 16) or
                   ((data[offset + 3].toInt() and 0xFF) shl 24)
        offset += 4
        return value
    }
    
    // Read SPK ID
    builder.setSpkId(readU32())
    
    // Read SPK public key (32 bytes)
    val spkPub = data.copyOfRange(offset, offset + 32)
    builder.setSignedPrekey(ByteString.copyFrom(spkPub))
    offset += 32
    
    // Read SPK signature
    val sigLen = readU32()
    val spkSig = data.copyOfRange(offset, offset + sigLen)
    builder.setSpkSignature(ByteString.copyFrom(spkSig))
    offset += sigLen
    
    // Read one-time prekeys
    val opkCount = readU32()
    for (i in 0 until opkCount) {
        val opkId = readU32()
        val opkPub = data.copyOfRange(offset, offset + 32)
        offset += 32
        
        builder.addOpkIds(opkId)
        builder.addOneTimePrekeys(ByteString.copyFrom(opkPub))
    }
    
    return builder.build()
}

// ============================================================================
// ByteArray Helpers
// ============================================================================

fun ByteArray.toHex(): String {
    return joinToString("") { "%02x".format(it) }
}

fun String.hexToByteArray(): ByteArray {
    require(length % 2 == 0) { "Hex string must have even length" }
    return chunked(2).map { it.toInt(16).toByte() }.toByteArray()
}
