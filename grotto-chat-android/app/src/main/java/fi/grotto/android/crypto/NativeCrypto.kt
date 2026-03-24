package fi.grotto.android.crypto

import android.util.Log

/**
 * JNI bridge to the shared C++ crypto engine (libsignal-protocol-c + libsodium).
 * 
 * This class provides Signal Protocol encryption (X3DH + Double Ratchet) for
 * end-to-end encrypted messaging. It uses a native library built with CMake/NDK.
 */
object NativeCrypto {
    private const val TAG = "NativeCrypto"
    
    init {
        try {
            System.loadLibrary("grotto-native")
            Log.i(TAG, "Native library loaded successfully")
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Failed to load native library", e)
        }
    }

    // ============================================================================
    // Data Classes
    // ============================================================================
    
    /**
     * Signed Pre-Key information for registration.
     */
    data class SpkInfo(
        val pub: ByteArray,
        val sig: ByteArray,
        val id: Int,
    ) {
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (javaClass != other?.javaClass) return false
            other as SpkInfo
            return id == other.id && 
                   pub.contentEquals(other.pub) && 
                   sig.contentEquals(other.sig)
        }
        
        override fun hashCode(): Int {
            var result = pub.contentHashCode()
            result = 31 * result + sig.contentHashCode()
            result = 31 * result + id
            return result
        }
    }

    // ============================================================================
    // Initialization
    // ============================================================================
    
    /**
     * Initialize the crypto engine with the given user ID and passphrase.
     * This loads or generates the Ed25519 identity key.
     * 
     * @param store Store implementation for persistent storage
     * @param userId The local user's ID
     * @param passphrase Passphrase to encrypt the identity private key
     * @return true if initialization succeeded
     */
    @JvmStatic
    external fun nativeInit(store: NativeStore, userId: String, passphrase: String): Boolean

    // ============================================================================
    // Identity & Registration
    // ============================================================================
    
    /**
     * Get the current identity public key (Ed25519, 32 bytes).
     */
    @JvmStatic
    external fun identityPub(): ByteArray?
    
    /**
     * Get the current signed pre-key info.
     */
    @JvmStatic
    external fun currentSpk(): SpkInfo?
    
    /**
     * Prepare registration data for the server.
     * Generates signed pre-key and one-time pre-keys.
     * 
     * @param numOpks Number of one-time pre-keys to generate (default 100)
     * @return Serialized KeyUpload protobuf bytes (simplified format)
     */
    @JvmStatic
    external fun prepareRegistration(numOpks: Int = 100): ByteArray?

    // ============================================================================
    // Encryption/Decryption (1:1)
    // ============================================================================
    
    /**
     * Encrypt a message for a recipient.
     * 
     * If no session exists, returns empty and caller should request key bundle.
     * 
     * @param recipientId Recipient user ID
     * @param plaintext The plaintext message bytes
     * @return Ciphertext bytes, or null/empty if no session (need key bundle)
     */
    @JvmStatic
    external fun encrypt(recipientId: String, plaintext: ByteArray): ByteArray?
    
    /**
     * Decrypt a message from a sender.
     * 
     * @param senderId The sender's user ID
     * @param recipientId The recipient (user_id or channel)
     * @param ciphertext The ciphertext bytes
     * @param type Ciphertext type (2=SIGNAL_MESSAGE, 3=PRE_KEY_SIGNAL_MESSAGE)
     * @return Decrypted plaintext bytes, or null if decryption failed
     */
    @JvmStatic
    external fun decrypt(senderId: String, recipientId: String, ciphertext: ByteArray, type: Int): ByteArray?
    
    /**
     * Process a key bundle to establish an X3DH session.
     * 
     * @param recipientId The recipient's user ID
     * @param bundleData The KeyBundle bytes from server
     */
    @JvmStatic
    external fun onKeyBundle(recipientId: String, bundleData: ByteArray)
    
    /**
     * Check if we have an established session with this recipient.
     */
    @JvmStatic
    external fun hasSession(recipientId: String): Boolean

    // ============================================================================
    // Group Encryption (Sender Keys)
    // ============================================================================
    
    /**
     * Initialize a group session for a channel.
     * 
     * @param channelId The channel ID (e.g., "#general")
     * @param members Array of member user IDs
     */
    @JvmStatic
    external fun initGroupSession(channelId: String, members: Array<String>)

    /**
     * Result of group encryption - contains ciphertext and optional SKDM for first message.
     */
    data class GroupEncryptResult(
        val ciphertext: ByteArray,
        val skdm: ByteArray?  // SenderKeyDistributionMessage (null if not first message)
    )

    /**
     * Encrypt a message for a group/channel.
     * 
     * @param channelId The channel ID
     * @param plaintext The plaintext message bytes
     * @return GroupEncryptResult containing ciphertext and optional SKDM
     */
    @JvmStatic
    external fun encryptGroup(channelId: String, plaintext: ByteArray): GroupEncryptResult?

    /**
     * Decrypt a group message.
     * 
     * @param senderId The sender's user ID
     * @param channelId The channel ID
     * @param ciphertext The ciphertext bytes
     * @param skdm Optional SenderKeyDistributionMessage (for first message from sender)
     * @return Decrypted plaintext bytes
     */
    @JvmStatic
    external fun decryptGroup(senderId: String, channelId: String, ciphertext: ByteArray, skdm: ByteArray? = null): ByteArray?
    
    /**
     * Process a Sender Key Distribution Message.
     * This establishes the sender key state needed to decrypt messages from this sender.
     * 
     * @param senderId The sender's user ID
     * @param channelId The channel ID
     * @param skdm The SenderKeyDistributionMessage bytes
     */
    @JvmStatic
    external fun processSenderKeyDistribution(senderId: String, channelId: String, skdm: ByteArray)

    // ============================================================================
    // Safety Number
    // ============================================================================
    
    /**
     * Compute the safety number for a peer.
     * This is a 60-digit number derived from both parties' identity keys,
     * used for out-of-band verification.
     * 
     * @param peerId The peer's user ID
     * @return Human-readable safety number string (60 digits in groups of 5)
     */
    @JvmStatic
    external fun safetyNumber(peerId: String): String

    // ============================================================================
    // Authentication
    // ============================================================================
    
    /**
     * Sign an authentication challenge.
     * 
     * @param nonce The challenge nonce from server (32 bytes)
     * @return 64-byte Ed25519 signature
     */
    @JvmStatic
    external fun signChallenge(nonce: ByteArray): ByteArray?

    // ============================================================================
    // Store Interface
    // ============================================================================
    
    /**
     * Storage interface that the native layer uses to persist data.
     * The Kotlin side implements this using Room/SQLite.
     * 
     * All methods are called from native code on the JNI thread.
     * Implementations should handle thread safety appropriately.
     */
    interface Store {
        // Identity
        /**
         * Save identity key pair (public key + encrypted private key).
         * 
         * @param userId The user ID
         * @param pubKey Ed25519 public key (32 bytes)
         * @param privKeyEncrypted Encrypted private key (Argon2id + XChaCha20-Poly1305)
         * @param salt Salt used for key derivation (16 bytes)
         */
        fun saveIdentity(userId: String, pubKey: ByteArray, privKeyEncrypted: ByteArray, salt: ByteArray)
        
        /**
         * Load identity key pair.
         * 
         * @param userId The user ID
         * @return Byte array format: [4 bytes salt_len LE] [4 bytes pub_len LE] [salt] [pub_key] [priv_encrypted]
         */
        fun loadIdentity(userId: String): ByteArray?
        
        // Sessions
        fun saveSession(address: String, record: ByteArray)
        fun loadSession(address: String): ByteArray?
        
        // Pre-keys
        fun savePreKey(id: Int, record: ByteArray)
        fun loadPreKey(id: Int): ByteArray?
        fun removePreKey(id: Int)
        
        // Signed pre-keys
        fun saveSignedPreKey(id: Int, record: ByteArray)
        fun loadSignedPreKey(id: Int): ByteArray?
        
        // Peer identities
        fun savePeerIdentity(userId: String, pubKey: ByteArray)
        fun loadPeerIdentity(userId: String): ByteArray?
        
        // Sender keys (group sessions)
        fun saveSenderKey(senderKeyId: String, record: ByteArray)
        fun loadSenderKey(senderKeyId: String): ByteArray?
    }
}
