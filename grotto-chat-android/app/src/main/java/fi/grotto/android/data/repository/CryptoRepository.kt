package fi.grotto.android.data.repository

import android.util.Log
import fi.grotto.android.crypto.NativeCrypto
import fi.grotto.android.crypto.NativeStore
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Repository for cryptographic operations.
 * 
 * Provides high-level access to Signal Protocol encryption/decryption
 * backed by the native C++ crypto engine.
 */
@Singleton
class CryptoRepository @Inject constructor(
    private val nativeStore: NativeStore
) {
    companion object {
        private const val TAG = "CryptoRepository"
        
        // Ciphertext types matching Signal Protocol
        const val CIPHERTYPE_SIGNAL_MESSAGE = 2        // Regular Signal message (Double Ratchet)
        const val CIPHERTYPE_PREKEY_MESSAGE = 3        // Initial X3DH message
        const val CIPHERTYPE_SENDER_KEY_MESSAGE = 4    // Group message (Sender Keys)
    }
    
    private var isInitialized = false
    private var currentUserId: String? = null
    
    /**
     * Initialize the crypto engine for the current user.
     * 
     * @param userId The current user's ID
     * @param passphrase The passphrase to decrypt the identity key
     * @return true if initialization succeeded
     */
    suspend fun initialize(userId: String, passphrase: String): Boolean = 
        withContext(Dispatchers.IO) {
            try {
                val result = NativeCrypto.nativeInit(nativeStore, userId, passphrase)
                if (result) {
                    isInitialized = true
                    currentUserId = userId
                    Log.i(TAG, "Crypto engine initialized for $userId")
                } else {
                    Log.e(TAG, "Failed to initialize crypto engine")
                }
                result
            } catch (e: Exception) {
                Log.e(TAG, "Exception initializing crypto engine", e)
                false
            }
        }
    
    /**
     * Check if the crypto engine is initialized.
     */
    fun isReady(): Boolean = isInitialized && NativeCrypto.currentSpk() != null
    
    /**
     * Get the current user's identity public key.
     */
    fun getIdentityPublicKey(): ByteArray? {
        if (!isInitialized) return null
        return NativeCrypto.identityPub()
    }
    
    /**
     * Get the current signed pre-key info for registration.
     */
    fun getSignedPreKey(): NativeCrypto.SpkInfo? {
        if (!isInitialized) return null
        return NativeCrypto.currentSpk()
    }
    
    /**
     * Prepare key upload data for server registration.
     * 
     * @param numOpks Number of one-time pre-keys to generate (default 100)
     * @return Serialized KeyUpload data
     */
    suspend fun prepareKeyUpload(numOpks: Int = 100): ByteArray? =
        withContext(Dispatchers.IO) {
            if (!isInitialized) return@withContext null
            NativeCrypto.prepareRegistration(numOpks)
        }
    
    /**
     * Encrypt a message for a recipient.
     * 
     * @param recipientId The recipient's user ID or channel ID
     * @param plaintext The plaintext message
     * @return Encrypted data, or null if no session exists (caller should request key bundle)
     */
    suspend fun encrypt(recipientId: String, plaintext: String): ByteArray? =
        withContext(Dispatchers.IO) {
            if (!isInitialized) return@withContext null
            NativeCrypto.encrypt(recipientId, plaintext.toByteArray(Charsets.UTF_8))
        }
    
    /**
     * Decrypt a message from a sender.
     * 
     * @param senderId The sender's user ID
     * @param recipientId The recipient (user ID or channel ID)
     * @param ciphertext The encrypted data
     * @param type The ciphertext type (2, 3, or 4)
     * @return Decrypted plaintext, or null if decryption failed
     */
    suspend fun decrypt(
        senderId: String, 
        recipientId: String, 
        ciphertext: ByteArray, 
        type: Int
    ): String? = withContext(Dispatchers.IO) {
        if (!isInitialized) return@withContext null
        
        val result = NativeCrypto.decrypt(senderId, recipientId, ciphertext, type)
        result?.toString(Charsets.UTF_8)
    }
    
    /**
     * Process a key bundle to establish a session with a recipient.
     * 
     * @param recipientId The recipient's user ID
     * @param bundleData The key bundle data from the server
     */
    suspend fun processKeyBundle(recipientId: String, bundleData: ByteArray) =
        withContext(Dispatchers.IO) {
            if (!isInitialized) return@withContext
            NativeCrypto.onKeyBundle(recipientId, bundleData)
        }
    
    /**
     * Check if we have an established session with a recipient.
     */
    fun hasSession(recipientId: String): Boolean {
        if (!isInitialized) return false
        return NativeCrypto.hasSession(recipientId)
    }
    
    // ============================================================================
    // Group Encryption
    // ============================================================================
    
    /**
     * Initialize a group session for a channel.
     * 
     * @param channelId The channel ID (e.g., "#general")
     * @param members List of member user IDs
     */
    suspend fun initGroupSession(channelId: String, members: List<String>) =
        withContext(Dispatchers.IO) {
            if (!isInitialized) return@withContext
            NativeCrypto.initGroupSession(channelId, members.toTypedArray())
        }
    
    /**
     * Encrypt a message for a group/channel.
     * 
     * @param channelId The channel ID
     * @param plaintext The plaintext message
     * @return GroupEncryptResult containing ciphertext and optional SKDM
     */
    suspend fun encryptGroup(channelId: String, plaintext: String): NativeCrypto.GroupEncryptResult? =
        withContext(Dispatchers.IO) {
            if (!isInitialized) return@withContext null
            NativeCrypto.encryptGroup(channelId, plaintext.toByteArray(Charsets.UTF_8))
        }
    
    /**
     * Decrypt a group message.
     * 
     * @param senderId The sender's user ID
     * @param channelId The channel ID
     * @param ciphertext The encrypted data
     * @param skdm Optional SenderKeyDistributionMessage (for first message from sender)
     * @return Decrypted plaintext
     */
    suspend fun decryptGroup(
        senderId: String, 
        channelId: String, 
        ciphertext: ByteArray,
        skdm: ByteArray? = null
    ): String? = withContext(Dispatchers.IO) {
        if (!isInitialized) return@withContext null
        
        val result = NativeCrypto.decryptGroup(senderId, channelId, ciphertext, skdm)
        result?.toString(Charsets.UTF_8)
    }
    
    /**
     * Process a Sender Key Distribution Message.
     */
    suspend fun processSenderKeyDistribution(
        senderId: String, 
        channelId: String, 
        skdm: ByteArray
    ) = withContext(Dispatchers.IO) {
        if (!isInitialized) return@withContext
        NativeCrypto.processSenderKeyDistribution(senderId, channelId, skdm)
    }
    
    // ============================================================================
    // Safety Number
    // ============================================================================
    
    /**
     * Compute the safety number for a peer.
     * 
     * @param peerId The peer's user ID
     * @return The 60-digit safety number formatted as "12345 67890 ..."
     */
    fun getSafetyNumber(peerId: String): String {
        if (!isInitialized) return ""
        return NativeCrypto.safetyNumber(peerId)
    }
    
    // ============================================================================
    // Authentication
    // ============================================================================
    
    /**
     * Sign an authentication challenge.
     * 
     * @param nonce The challenge nonce (32 bytes)
     * @return The 64-byte signature
     */
    suspend fun signChallenge(nonce: ByteArray): ByteArray? =
        withContext(Dispatchers.IO) {
            if (!isInitialized) return@withContext null
            NativeCrypto.signChallenge(nonce)
        }
    
    // ============================================================================
    // Cleanup
    // ============================================================================
    
    /**
     * Clear all crypto data (for logout).
     */
    suspend fun clearAll() = withContext(Dispatchers.IO) {
        nativeStore.clearAll()
        isInitialized = false
        currentUserId = null
        Log.i(TAG, "Crypto data cleared")
    }
}
