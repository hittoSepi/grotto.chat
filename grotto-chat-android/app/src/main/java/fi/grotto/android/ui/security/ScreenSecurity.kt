package fi.grotto.android.ui.security

import android.app.Activity
import android.view.WindowManager
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.ui.platform.LocalContext

/**
 * Screen security utilities for preventing screen capture/screenshots.
 * Uses FLAG_SECURE to prevent screenshots and screen recording.
 */
object ScreenSecurity {

    /**
     * Enable FLAG_SECURE for the current window.
     * This prevents screenshots and screen recording.
     */
    fun enable(activity: Activity) {
        activity.window.setFlags(
            WindowManager.LayoutParams.FLAG_SECURE,
            WindowManager.LayoutParams.FLAG_SECURE
        )
    }

    /**
     * Disable FLAG_SECURE for the current window.
     */
    fun disable(activity: Activity) {
        activity.window.clearFlags(WindowManager.LayoutParams.FLAG_SECURE)
    }
}

/**
 * Composable that enables screen security (FLAG_SECURE) while in composition.
 * 
 * Usage:
 * ```
 * @Composable
 * fun ChatScreen(
 *     viewModel: ChatViewModel = hiltViewModel()
 * ) {
 *     val screenCaptureEnabled by viewModel.screenCaptureEnabled.collectAsState()
 *     SecureScreenEffect(enabled = screenCaptureEnabled)
 *     // ... chat content
 * }
 * ```
 * 
 * @param enabled Whether to enable screen security. Should come from user preferences.
 */
@Composable
fun SecureScreenEffect(enabled: Boolean = true) {
    val context = LocalContext.current
    val activity = context as? Activity

    DisposableEffect(enabled) {
        if (enabled && activity != null) {
            ScreenSecurity.enable(activity)
        }

        onDispose {
            // Clear the flag when leaving the screen
            if (activity != null) {
                ScreenSecurity.disable(activity)
            }
        }
    }
}
