package fi.grotto.android

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import dagger.hilt.android.AndroidEntryPoint
import fi.grotto.android.ui.navigation.GrottoNavGraph
import fi.grotto.android.ui.screenshot.ScreenshotSceneScreen
import fi.grotto.android.ui.screenshot.screenshotConfigFromIntent
import fi.grotto.android.ui.theme.FontScaleViewModel
import fi.grotto.android.ui.theme.GrottoTheme
import fi.grotto.android.ui.theme.ThemeViewModel

@AndroidEntryPoint
class MainActivity : ComponentActivity() {

    private val themeViewModel: ThemeViewModel by viewModels()
    private val fontScaleViewModel: FontScaleViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        val screenshotConfig = screenshotConfigFromIntent(intent)
        setContent {
            if (screenshotConfig != null) {
                GrottoAppTheme(
                    isDarkTheme = screenshotConfig.darkTheme,
                    fontScale = screenshotConfig.fontScale,
                ) {
                    Surface(modifier = Modifier.fillMaxSize()) {
                        ScreenshotSceneScreen(config = screenshotConfig)
                    }
                }
            } else {
                val isDarkTheme = themeViewModel.isDarkTheme.collectAsStateWithLifecycle().value
                val fontScale = fontScaleViewModel.fontScale.collectAsStateWithLifecycle().value

                GrottoAppTheme(
                    isDarkTheme = isDarkTheme,
                    fontScale = fontScale,
                ) {
                    Surface(modifier = Modifier.fillMaxSize()) {
                        GrottoNavGraph()
                    }
                }
            }
        }
    }
}

/**
 * Wrapper that handles theme mode (system/light/dark) and font scaling.
 * If isDarkTheme is null, follows system setting.
 */
@Composable
private fun GrottoAppTheme(
    isDarkTheme: Boolean?,
    fontScale: Float,
    content: @Composable () -> Unit,
) {
    val darkTheme = when (isDarkTheme) {
        true -> true
        false -> false
        null -> isSystemInDarkTheme() // Follow system
    }
    
    GrottoTheme(
        darkTheme = darkTheme,
        fontScale = fontScale,
        content = content,
    )
}
