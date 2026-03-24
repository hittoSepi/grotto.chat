package fi.grotto.android.security.database

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import androidx.core.content.edit
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import timber.log.Timber
import java.security.KeyStore
import java.security.SecureRandom
import java.util.Base64
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/**
 * Manages the encryption key for the SQLCipher database.
 * 
 * The database encryption key is:
 * 1. Generated randomly (256-bit)
 * 2. Stored encrypted using Android Keystore (hardware-backed when available)
 * 3. Or stored in EncryptedSharedPreferences as fallback
 * 
 * This ensures the database is encrypted at rest and the key is protected
 * by the Android security hardware.
 */
class DatabaseEncryptionManager(
    private val context: Context,
) {
    private val prefs by lazy {
        createEncryptedPreferences()
    }

    /**
     * Get or create the database encryption key.
     * 
     * @return The encryption key as a string (for SQLCipher)
     */
    suspend fun getOrCreateKey(): String = withContext(Dispatchers.IO) {
        val existingKey = getExistingKey()
        if (existingKey != null) {
            Timber.d("Using existing database encryption key")
            return@withContext existingKey
        }

        Timber.i("Generating new database encryption key")
        val newKey = generateRandomKey()
        storeKey(newKey)
        newKey
    }

    /**
     * Get existing key if available.
     */
    private fun getExistingKey(): String? {
        return try {
            val encryptedKey = prefs.getString(PREF_ENCRYPTED_KEY, null)
            val iv = prefs.getString(PREF_IV, null)
            
            if (encryptedKey != null && iv != null) {
                decryptKey(encryptedKey, iv)
            } else {
                null
            }
        } catch (e: Exception) {
            Timber.e(e, "Failed to retrieve existing key")
            null
        }
    }

    /**
     * Generate a random 256-bit key.
     */
    private fun generateRandomKey(): String {
        val keyBytes = ByteArray(32) // 256 bits
        SecureRandom().nextBytes(keyBytes)
        // Convert to hex string for SQLCipher
        return keyBytes.joinToString("") { "%02x".format(it) }
    }

    /**
     * Store the key securely.
     */
    private fun storeKey(key: String) {
        val masterKey = getOrCreateMasterKey()
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, masterKey)
        
        val encrypted = cipher.doFinal(key.toByteArray(Charsets.UTF_8))
        val iv = Base64.getEncoder().encodeToString(cipher.iv)
        val encryptedKey = Base64.getEncoder().encodeToString(encrypted)
        
        prefs.edit {
            putString(PREF_ENCRYPTED_KEY, encryptedKey)
            putString(PREF_IV, iv)
        }
        
        Timber.i("Database encryption key stored securely")
    }

    /**
     * Decrypt the stored key.
     */
    private fun decryptKey(encryptedKey: String, iv: String): String {
        val masterKey = getOrCreateMasterKey()
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        val ivSpec = GCMParameterSpec(128, Base64.getDecoder().decode(iv))
        cipher.init(Cipher.DECRYPT_MODE, masterKey, ivSpec)
        
        val decrypted = cipher.doFinal(Base64.getDecoder().decode(encryptedKey))
        return String(decrypted, Charsets.UTF_8)
    }

    /**
     * Get or create the master key in Android Keystore.
     */
    private fun getOrCreateMasterKey(): SecretKey {
        val keyStore = KeyStore.getInstance("AndroidKeyStore")
        keyStore.load(null)
        
        // Try to get existing key
        val existingKey = keyStore.getEntry(KEY_ALIAS, null) as? KeyStore.SecretKeyEntry
        if (existingKey != null) {
            return existingKey.secretKey
        }
        
        // Create new key
        Timber.i("Creating new master key in Android Keystore")
        val keyGen = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore")
        val spec = KeyGenParameterSpec.Builder(
            KEY_ALIAS,
            KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT
        )
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setKeySize(256)
            .setUserAuthenticationRequired(false) // Can be changed for biometric
            .build()
        
        keyGen.init(spec)
        return keyGen.generateKey()
    }

    /**
     * Create encrypted preferences for storing the encrypted key.
     */
    private fun createEncryptedPreferences(): androidx.security.crypto.EncryptedSharedPreferences {
        val masterKey = MasterKey.Builder(context)
            .setKeyScheme(MasterKey.KeyScheme.AES256_GCM)
            .build()
        
        return EncryptedSharedPreferences.create(
            context,
            PREFS_NAME,
            masterKey,
            EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
            EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
        ) as androidx.security.crypto.EncryptedSharedPreferences
    }

    /**
     * Change the database encryption key.
     * WARNING: This requires re-encrypting the entire database.
     */
    suspend fun changeKey(): String = withContext(Dispatchers.IO) {
        Timber.w("Changing database encryption key - this requires re-encryption")
        
        // Generate new key
        val newKey = generateRandomKey()
        
        // Store new key
        storeKey(newKey)
        
        Timber.i("Database encryption key changed successfully")
        newKey
    }

    /**
     * Check if a key exists.
     */
    fun hasKey(): Boolean {
        return prefs.contains(PREF_ENCRYPTED_KEY)
    }

    /**
     * Delete the stored key (DANGEROUS - will make database inaccessible).
     */
    fun deleteKey() {
        prefs.edit {
            remove(PREF_ENCRYPTED_KEY)
            remove(PREF_IV)
        }
        Timber.w("Database encryption key deleted")
    }

    companion object {
        private const val PREFS_NAME = "database_key_prefs"
        private const val KEY_ALIAS = "grotto_database_master_key"
        private const val PREF_ENCRYPTED_KEY = "encrypted_database_key"
        private const val PREF_IV = "database_key_iv"
    }
}
