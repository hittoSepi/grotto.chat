package fi.grotto.android.security.biometric

import android.content.Context
import androidx.biometric.BiometricManager
import androidx.biometric.BiometricPrompt
import androidx.core.content.ContextCompat
import androidx.fragment.app.FragmentActivity
import dagger.hilt.android.qualifiers.ApplicationContext
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.suspendCancellableCoroutine
import timber.log.Timber
import javax.inject.Inject
import javax.inject.Singleton
import kotlin.coroutines.resume

/**
 * Manages biometric authentication for cryptographic operations.
 * 
 * This manager handles:
 * - Checking biometric availability
 * - Prompting for biometric authentication
 * - Managing biometric-protected keys
 * - Storing user preference for biometric auth
 */
@Singleton
class BiometricAuthManager @Inject constructor(
    @ApplicationContext private val context: Context,
) {
    private val biometricManager = BiometricManager.from(context)
    
    private val _authState = MutableStateFlow<BiometricAuthState>(BiometricAuthState.NotAuthenticated)
    val authState: StateFlow<BiometricAuthState> = _authState.asStateFlow()
    
    /**
     * Check if biometric authentication is available on this device.
     */
    fun canAuthenticate(): BiometricAvailability {
        return when (biometricManager.canAuthenticate(BiometricManager.Authenticators.BIOMETRIC_STRONG)) {
            BiometricManager.BIOMETRIC_SUCCESS -> {
                Timber.d("Biometric authentication is available")
                BiometricAvailability.Available
            }
            BiometricManager.BIOMETRIC_ERROR_HW_UNAVAILABLE -> {
                Timber.w("Biometric hardware unavailable")
                BiometricAvailability.HardwareUnavailable
            }
            BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED -> {
                Timber.w("No biometric credentials enrolled")
                BiometricAvailability.NotEnrolled
            }
            BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE -> {
                Timber.w("No biometric hardware")
                BiometricAvailability.NoHardware
            }
            BiometricManager.BIOMETRIC_ERROR_SECURITY_UPDATE_REQUIRED -> {
                Timber.w("Security update required")
                BiometricAvailability.SecurityUpdateRequired
            }
            else -> {
                Timber.w("Biometric authentication not available")
                BiometricAvailability.Unknown
            }
        }
    }
    
    /**
     * Check if strong biometric authentication is available (for crypto operations).
     */
    fun isStrongBiometricAvailable(): Boolean {
        return biometricManager.canAuthenticate(BiometricManager.Authenticators.BIOMETRIC_STRONG) == 
               BiometricManager.BIOMETRIC_SUCCESS
    }
    
    /**
     * Authenticate with biometrics.
     * 
     * @param activity The FragmentActivity to show the prompt on
     * @param title The title for the biometric prompt
     * @param subtitle The subtitle for the biometric prompt
     * @return true if authentication succeeded
     */
    suspend fun authenticate(
        activity: FragmentActivity,
        title: String = "Authenticate",
        subtitle: String = "Confirm your identity to access secure data",
        description: String = "Use your biometric credential",
    ): Boolean = suspendCancellableCoroutine { continuation ->
        
        if (!isStrongBiometricAvailable()) {
            Timber.w("Biometric authentication not available")
            _authState.value = BiometricAuthState.Error("Biometric authentication not available")
            continuation.resume(false)
            return@suspendCancellableCoroutine
        }
        
        _authState.value = BiometricAuthState.Authenticating
        
        val executor = ContextCompat.getMainExecutor(activity)
        
        val callback = object : BiometricPrompt.AuthenticationCallback() {
            override fun onAuthenticationError(errorCode: Int, errString: CharSequence) {
                super.onAuthenticationError(errorCode, errString)
                Timber.e("Biometric authentication error: $errorCode - $errString")
                _authState.value = BiometricAuthState.Error(errString.toString())
                continuation.resume(false)
            }
            
            override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult) {
                super.onAuthenticationSucceeded(result)
                Timber.i("Biometric authentication succeeded")
                _authState.value = BiometricAuthState.Authenticated
                continuation.resume(true)
            }
            
            override fun onAuthenticationFailed() {
                super.onAuthenticationFailed()
                Timber.w("Biometric authentication failed")
                // Don't change state or resume here - wait for error or success
            }
        }
        
        val biometricPrompt = BiometricPrompt(activity, executor, callback)
        
        val promptInfo = BiometricPrompt.PromptInfo.Builder()
            .setTitle(title)
            .setSubtitle(subtitle)
            .setDescription(description)
            .setAllowedAuthenticators(BiometricManager.Authenticators.BIOMETRIC_STRONG)
            .setNegativeButtonText("Cancel")
            .build()
        
        biometricPrompt.authenticate(promptInfo)
        
        continuation.invokeOnCancellation {
            biometricPrompt.cancelAuthentication()
        }
    }
    
    /**
     * Authenticate with biometrics for crypto operations.
     * This uses crypto-bound authentication when available.
     */
    suspend fun authenticateForCrypto(
        activity: FragmentActivity,
        cryptoObject: BiometricPrompt.CryptoObject,
        title: String = "Authenticate",
        subtitle: String = "Confirm to decrypt your identity key",
    ): Boolean = suspendCancellableCoroutine { continuation ->
        
        if (!isStrongBiometricAvailable()) {
            Timber.w("Biometric authentication not available")
            continuation.resume(false)
            return@suspendCancellableCoroutine
        }
        
        _authState.value = BiometricAuthState.Authenticating
        
        val executor = ContextCompat.getMainExecutor(activity)
        
        val callback = object : BiometricPrompt.AuthenticationCallback() {
            override fun onAuthenticationError(errorCode: Int, errString: CharSequence) {
                super.onAuthenticationError(errorCode, errString)
                Timber.e("Biometric authentication error: $errorCode - $errString")
                _authState.value = BiometricAuthState.Error(errString.toString())
                continuation.resume(false)
            }
            
            override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult) {
                super.onAuthenticationSucceeded(result)
                Timber.i("Biometric authentication succeeded for crypto")
                _authState.value = BiometricAuthState.Authenticated
                continuation.resume(true)
            }
            
            override fun onAuthenticationFailed() {
                super.onAuthenticationFailed()
                Timber.w("Biometric authentication failed")
            }
        }
        
        val biometricPrompt = BiometricPrompt(activity, executor, callback)
        
        val promptInfo = BiometricPrompt.PromptInfo.Builder()
            .setTitle(title)
            .setSubtitle(subtitle)
            .setAllowedAuthenticators(BiometricManager.Authenticators.BIOMETRIC_STRONG)
            .setNegativeButtonText("Cancel")
            .build()
        
        biometricPrompt.authenticate(promptInfo, cryptoObject)
        
        continuation.invokeOnCancellation {
            biometricPrompt.cancelAuthentication()
        }
    }
    
    /**
     * Check if user has enabled biometric authentication for identity key.
     */
    fun isBiometricEnabled(): Boolean {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return prefs.getBoolean(PREF_BIOMETRIC_ENABLED, false)
    }
    
    /**
     * Enable or disable biometric authentication.
     */
    fun setBiometricEnabled(enabled: Boolean) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putBoolean(PREF_BIOMETRIC_ENABLED, enabled)
            .apply()
        
        Timber.i("Biometric authentication ${if (enabled) "enabled" else "disabled"}")
    }
    
    /**
     * Reset authentication state.
     */
    fun resetAuthState() {
        _authState.value = BiometricAuthState.NotAuthenticated
    }
    
    companion object {
        private const val PREFS_NAME = "biometric_prefs"
        private const val PREF_BIOMETRIC_ENABLED = "biometric_enabled"
    }
}

/**
 * Represents the availability of biometric authentication.
 */
sealed class BiometricAvailability {
    object Available : BiometricAvailability()
    object NoHardware : BiometricAvailability()
    object HardwareUnavailable : BiometricAvailability()
    object NotEnrolled : BiometricAvailability()
    object SecurityUpdateRequired : BiometricAvailability()
    object Unknown : BiometricAvailability()
    
    fun isAvailable(): Boolean = this is Available
}

/**
 * Represents the current biometric authentication state.
 */
sealed class BiometricAuthState {
    object NotAuthenticated : BiometricAuthState()
    object Authenticating : BiometricAuthState()
    object Authenticated : BiometricAuthState()
    data class Error(val message: String) : BiometricAuthState()
}
