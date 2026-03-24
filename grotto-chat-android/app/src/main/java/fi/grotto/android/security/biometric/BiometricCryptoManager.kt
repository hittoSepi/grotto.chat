package fi.grotto.android.security.biometric

import androidx.fragment.app.FragmentActivity
import fi.grotto.android.crypto.NativeCrypto
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow
import timber.log.Timber
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Manager that wraps cryptographic operations with biometric authentication.
 * 
 * This ensures that sensitive crypto operations (decrypting identity key, 
 * signing messages) require biometric authentication when enabled.
 */
@Singleton
class BiometricCryptoManager @Inject constructor(
    private val biometricAuthManager: BiometricAuthManager,
) {
    /**
     * Check if biometric authentication is required for crypto operations.
     */
    fun isBiometricRequired(): Boolean {
        return biometricAuthManager.isBiometricEnabled() && 
               biometricAuthManager.isStrongBiometricAvailable()
    }
    
    /**
     * Perform a crypto operation with optional biometric authentication.
     * 
     * @param activity The activity to show biometric prompt on
     * @param operation The crypto operation to perform
     * @return Result of the operation, or null if authentication failed
     */
    suspend fun <T> performWithBiometric(
        activity: FragmentActivity,
        operationName: String = "crypto operation",
        operation: suspend () -> T,
    ): T? {
        // Check if biometrics is required
        if (!isBiometricRequired()) {
            // Biometrics not enabled or not available, proceed without
            return operation()
        }
        
        // Check if already authenticated
        if (biometricAuthManager.authState.value is BiometricAuthState.Authenticated) {
            Timber.d("Already authenticated, proceeding with $operationName")
            return operation()
        }
        
        // Need to authenticate
        Timber.d("Requesting biometric authentication for $operationName")
        
        val success = biometricAuthManager.authenticate(
            activity = activity,
            title = "Authentication Required",
            subtitle = "Authenticate to $operationName",
        )
        
        return if (success) {
            Timber.d("Biometric authentication successful, proceeding with $operationName")
            operation()
        } else {
            Timber.w("Biometric authentication failed for $operationName")
            null
        }
    }
    
    /**
     * Initialize crypto with biometric protection.
     * 
     * This authenticates the user before initializing the crypto engine,
     * ensuring the identity key is only loaded after biometric verification.
     */
    suspend fun initializeWithBiometric(
        activity: FragmentActivity,
        store: fi.grotto.android.crypto.NativeStore,
        userId: String,
        passphrase: String,
    ): Boolean {
        return performWithBiometric(
            activity = activity,
            operationName = "initialize encryption"
        ) {
            NativeCrypto.nativeInit(store, userId, passphrase)
        } ?: false
    }
    
    /**
     * Decrypt a message with biometric protection.
     */
    suspend fun decryptWithBiometric(
        activity: FragmentActivity,
        senderId: String,
        recipientId: String,
        ciphertext: ByteArray,
        type: Int,
    ): ByteArray? {
        return performWithBiometric(
            activity = activity,
            operationName = "decrypt message"
        ) {
            NativeCrypto.decrypt(senderId, recipientId, ciphertext, type)
        }
    }
    
    /**
     * Sign a challenge with biometric protection.
     */
    suspend fun signWithBiometric(
        activity: FragmentActivity,
        nonce: ByteArray,
    ): ByteArray? {
        return performWithBiometric(
            activity = activity,
            operationName = "sign authentication challenge"
        ) {
            NativeCrypto.signChallenge(nonce)
        }
    }
    
    /**
     * Get authentication state as a Flow.
     */
    fun observeAuthState(): Flow<BiometricAuthState> = biometricAuthManager.authState
    
    /**
     * Reset authentication state (require re-authentication).
     */
    fun requireReauthentication() {
        biometricAuthManager.resetAuthState()
        Timber.d("Biometric authentication state reset")
    }
}
