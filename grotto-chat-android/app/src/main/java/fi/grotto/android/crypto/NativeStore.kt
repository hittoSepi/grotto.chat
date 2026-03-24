package fi.grotto.android.crypto

import android.content.Context
import android.util.Base64
import android.util.Log
import fi.grotto.android.data.local.GrottoDatabase
import fi.grotto.android.data.local.dao.PeerIdentityDao
import fi.grotto.android.data.local.entity.PeerIdentityEntity
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Room-based implementation of NativeCrypto storage interface.
 * 
 * This class bridges the native crypto engine's storage needs with the app's
 * Room database. All operations are blocking (for JNI compatibility) but run
 * on the IO dispatcher to avoid blocking the main thread.
 * 
 * Note: JNI calls come from the native thread, so we use runBlocking to
 * bridge from synchronous JNI to suspending Room operations.
 */
@Singleton
class NativeStore @Inject constructor(
    private val context: Context,
    private val database: GrottoDatabase
) : NativeCrypto.Store {
    
    companion object {
        private const val PREFS_NAME = "grotto_crypto_prefs"
        private const val KEY_IDENTITY_PREFIX = "identity_"
        private const val KEY_SESSION_PREFIX = "session_"
        private const val KEY_PREKEY_PREFIX = "prekey_"
        private const val KEY_SIGNED_PREKEY_PREFIX = "signed_prekey_"
        private const val KEY_SENDER_KEY_PREFIX = "sender_key_"
        private const val TAG = "NativeStore"
    }
    
    private val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    private val peerIdentityDao: PeerIdentityDao = database.peerIdentityDao()
    
    // ============================================================================
    // Identity
    // ============================================================================
    
    override fun saveIdentity(
        userId: String, 
        pubKey: ByteArray, 
        privKeyEncrypted: ByteArray, 
        salt: ByteArray
    ) {
        runBlocking(Dispatchers.IO) {
            // Store in SharedPreferences for quick access
            // Format: [4 bytes pub_len LE] [4 bytes salt_len LE] [salt] [pub_key] [priv_encrypted]
            // Must match JNI bridge in jni_bridge.cpp
            val data = ByteArray(8 + salt.size + pubKey.size + privKeyEncrypted.size)
            var offset = 0
            
            // Write lengths (little-endian) - pub_len first to match JNI
            data[offset++] = (pubKey.size and 0xFF).toByte()
            data[offset++] = ((pubKey.size shr 8) and 0xFF).toByte()
            data[offset++] = ((pubKey.size shr 16) and 0xFF).toByte()
            data[offset++] = ((pubKey.size shr 24) and 0xFF).toByte()
            
            data[offset++] = (salt.size and 0xFF).toByte()
            data[offset++] = ((salt.size shr 8) and 0xFF).toByte()
            data[offset++] = ((salt.size shr 16) and 0xFF).toByte()
            data[offset++] = ((salt.size shr 24) and 0xFF).toByte()
            
            // Copy data - salt first, then pub_key, then priv_encrypted
            System.arraycopy(salt, 0, data, offset, salt.size)
            offset += salt.size
            System.arraycopy(pubKey, 0, data, offset, pubKey.size)
            offset += pubKey.size
            System.arraycopy(privKeyEncrypted, 0, data, offset, privKeyEncrypted.size)
            
            prefs.edit()
                .putString(KEY_IDENTITY_PREFIX + userId, data.toBase64())
                .apply()
            
            Log.d(TAG, "Saved identity for $userId")
        }
    }
    
    override fun loadIdentity(userId: String): ByteArray? {
        return runBlocking(Dispatchers.IO) {
            val base64 = prefs.getString(KEY_IDENTITY_PREFIX + userId, null)
            if (base64 == null) {
                Log.d(TAG, "No identity found for $userId")
                return@runBlocking null
            }
            
            val data = base64.fromBase64()
            Log.d(TAG, "Loaded identity for $userId (${data.size} bytes)")
            data
        }
    }
    
    // ============================================================================
    // Sessions
    // ============================================================================
    
    override fun saveSession(address: String, record: ByteArray) {
        runBlocking(Dispatchers.IO) {
            prefs.edit()
                .putString(KEY_SESSION_PREFIX + address, record.toBase64())
                .apply()
            Log.d(TAG, "Saved session for $address")
        }
    }
    
    override fun loadSession(address: String): ByteArray? {
        return runBlocking(Dispatchers.IO) {
            val base64 = prefs.getString(KEY_SESSION_PREFIX + address, null)
            if (base64 == null) {
                Log.d(TAG, "No session found for $address")
                return@runBlocking null
            }
            base64.fromBase64()
        }
    }
    
    // ============================================================================
    // Pre-keys
    // ============================================================================
    
    override fun savePreKey(id: Int, record: ByteArray) {
        runBlocking(Dispatchers.IO) {
            prefs.edit()
                .putString(KEY_PREKEY_PREFIX + id, record.toBase64())
                .apply()
            Log.d(TAG, "Saved pre-key $id")
        }
    }
    
    override fun loadPreKey(id: Int): ByteArray? {
        return runBlocking(Dispatchers.IO) {
            prefs.getString(KEY_PREKEY_PREFIX + id, null)?.fromBase64()
        }
    }
    
    override fun removePreKey(id: Int) {
        runBlocking(Dispatchers.IO) {
            prefs.edit()
                .remove(KEY_PREKEY_PREFIX + id)
                .apply()
            Log.d(TAG, "Removed pre-key $id")
        }
    }
    
    // ============================================================================
    // Signed Pre-keys
    // ============================================================================
    
    override fun saveSignedPreKey(id: Int, record: ByteArray) {
        runBlocking(Dispatchers.IO) {
            prefs.edit()
                .putString(KEY_SIGNED_PREKEY_PREFIX + id, record.toBase64())
                .apply()
            Log.d(TAG, "Saved signed pre-key $id")
        }
    }
    
    override fun loadSignedPreKey(id: Int): ByteArray? {
        return runBlocking(Dispatchers.IO) {
            prefs.getString(KEY_SIGNED_PREKEY_PREFIX + id, null)?.fromBase64()
        }
    }
    
    // ============================================================================
    // Peer Identities (using Room database)
    // ============================================================================
    
    override fun savePeerIdentity(userId: String, pubKey: ByteArray) {
        runBlocking(Dispatchers.IO) {
            // Store the Base64 public key for native store access
            val entity = PeerIdentityEntity(
                userId = userId,
                identityPub = pubKey,
                trustStatus = "unverified",
                safetyNumber = null,
                publicKey = pubKey.toBase64()
            )
            peerIdentityDao.insert(entity)
            Log.d(TAG, "Saved peer identity for $userId")
        }
    }
    
    override fun loadPeerIdentity(userId: String): ByteArray? {
        return runBlocking(Dispatchers.IO) {
            val entity = peerIdentityDao.getByUserId(userId)
            if (entity != null) {
                // Prefer the publicKey Base64 field if available, otherwise use identityPub
                entity.publicKey?.fromBase64() ?: entity.identityPub
            } else {
                Log.d(TAG, "No peer identity found for $userId")
                null
            }
        }
    }
    
    // ============================================================================
    // Sender Keys (Group Sessions)
    // ============================================================================
    
    override fun saveSenderKey(senderKeyId: String, record: ByteArray) {
        runBlocking(Dispatchers.IO) {
            prefs.edit()
                .putString(KEY_SENDER_KEY_PREFIX + senderKeyId, record.toBase64())
                .apply()
            Log.d(TAG, "Saved sender key $senderKeyId")
        }
    }
    
    override fun loadSenderKey(senderKeyId: String): ByteArray? {
        return runBlocking(Dispatchers.IO) {
            prefs.getString(KEY_SENDER_KEY_PREFIX + senderKeyId, null)?.fromBase64()
        }
    }
    
    // ============================================================================
    // Helper Methods
    // ============================================================================
    
    private fun ByteArray.toBase64(): String {
        return Base64.encodeToString(this, Base64.NO_WRAP)
    }
    
    private fun String.fromBase64(): ByteArray {
        return Base64.decode(this, Base64.NO_WRAP)
    }
    
    /**
     * Clear all crypto data (for logout/account deletion).
     */
    fun clearAll() {
        runBlocking(Dispatchers.IO) {
            prefs.edit().clear().apply()
            Log.d(TAG, "Cleared all crypto data")
        }
    }
}
