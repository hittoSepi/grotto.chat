package fi.grotto.android.security.biometric

import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.fragment.app.FragmentActivity
import kotlinx.coroutines.launch

/**
 * Composable that triggers biometric authentication.
 * 
 * Usage:
 * ```kotlin
 * var showBiometricPrompt by remember { mutableStateOf(false) }
 * 
 * if (showBiometricPrompt) {
 *     BiometricPrompt(
 *         title = "Unlock Identity Key",
 *         subtitle = "Use your fingerprint to decrypt",
 *         onSuccess = { 
 *             // Proceed with crypto operation
 *             showBiometricPrompt = false
 *         },
 *         onError = { error ->
 *             // Handle error
 *             showBiometricPrompt = false
 *         },
 *         onDismiss = {
 *             showBiometricPrompt = false
 *         }
 *     )
 * }
 * ```
 */
@Composable
fun BiometricPrompt(
    title: String = "Authenticate",
    subtitle: String = "Confirm your identity",
    description: String = "Use your biometric credential",
    onSuccess: () -> Unit,
    onError: (String) -> Unit,
    onDismiss: () -> Unit,
) {
    val context = LocalContext.current
    
    LaunchedEffect(Unit) {
        val activity = context as? FragmentActivity
        if (activity == null) {
            onError("Biometric authentication requires FragmentActivity")
            return@LaunchedEffect
        }
        
        val manager = BiometricAuthManager(activity)
        
        launch {
            val success = manager.authenticate(
                activity = activity,
                title = title,
                subtitle = subtitle,
                description = description
            )
            
            if (success) {
                onSuccess()
            } else {
                onDismiss()
            }
        }
    }
}

/**
 * Check if biometric authentication is available.
 * 
 * @return true if biometrics can be used
 */
@Composable
fun rememberBiometricAvailability(): Boolean {
    val context = LocalContext.current
    return remember {
        val manager = BiometricAuthManager(context)
        manager.canAuthenticate().isAvailable()
    }
}

/**
 * State holder for biometric authentication in a screen.
 */
class BiometricAuthStateHolder(
    private val manager: BiometricAuthManager,
) {
    var isAuthenticated by mutableStateOf(false)
        private set
    
    var isPromptVisible by mutableStateOf(false)
        private set
    
    var lastError by mutableStateOf<String?>(null)
        private set
    
    fun showPrompt() {
        if (!manager.canAuthenticate().isAvailable()) {
            lastError = "Biometric authentication not available"
            return
        }
        isPromptVisible = true
    }
    
    fun hidePrompt() {
        isPromptVisible = false
    }
    
    fun onAuthenticated() {
        isAuthenticated = true
        isPromptVisible = false
        lastError = null
    }
    
    fun onError(error: String) {
        lastError = error
        isPromptVisible = false
    }
    
    fun reset() {
        isAuthenticated = false
        isPromptVisible = false
        lastError = null
        manager.resetAuthState()
    }
}

/**
 * Create a state holder for biometric authentication.
 */
@Composable
fun rememberBiometricAuthState(): BiometricAuthStateHolder {
    val context = LocalContext.current
    return remember {
        val manager = BiometricAuthManager(context)
        BiometricAuthStateHolder(manager)
    }
}
