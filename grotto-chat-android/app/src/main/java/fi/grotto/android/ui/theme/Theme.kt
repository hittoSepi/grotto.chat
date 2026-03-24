package fi.grotto.android.ui.theme

import androidx.compose.animation.core.CubicBezierEasing
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.unit.dp

val LocalGrottoColors = staticCompositionLocalOf { DarkGrottoColors }

object GrottoMotion {
    val durationFast = 150
    val durationMedium = 300
    val durationSlow = 500

    val easingStandard = CubicBezierEasing(0.2f, 0.0f, 0.0f, 1.0f)
    val easingDecelerate = CubicBezierEasing(0.0f, 0.0f, 0.0f, 1.0f)
    val easingAccelerate = CubicBezierEasing(0.3f, 0.0f, 1.0f, 1.0f)

    val speakingPulseInterval = 100
    val speakingGlowRadius = 4.dp
}

object GrottoElevation {
    val level0 = 0.dp
    val level1 = 1.dp
    val level2 = 3.dp
    val level3 = 6.dp
    val level4 = 8.dp
    val level5 = 12.dp
}

@Composable
fun GrottoTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    fontScale: Float = 1.0f,
    content: @Composable () -> Unit,
) {
    val colorScheme = if (darkTheme) DarkColorScheme else LightColorScheme
    val semanticColors = if (darkTheme) DarkGrottoColors else LightGrottoColors
    val typography = createScaledTypography(fontScale)

    CompositionLocalProvider(LocalGrottoColors provides semanticColors) {
        MaterialTheme(
            colorScheme = colorScheme,
            typography = typography,
            shapes = GrottoShapes,
            content = content,
        )
    }
}

object GrottoTheme {
    val semanticColors: GrottoSemanticColors
        @Composable get() = LocalGrottoColors.current
}
