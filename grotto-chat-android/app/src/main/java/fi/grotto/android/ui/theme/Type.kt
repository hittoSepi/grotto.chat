package fi.grotto.android.ui.theme

import androidx.compose.material3.Typography
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp

val MonoFamily = FontFamily.Monospace

val MonoStyle = TextStyle(
    fontFamily = MonoFamily,
    fontSize = 14.sp,
    lineHeight = 20.sp,
    letterSpacing = 0.sp,
)

private fun scaledSp(
    base: Int,
    scale: Float,
    minimum: Int,
) = (base.toFloat() * scale).coerceAtLeast(minimum.toFloat()).sp

/**
 * Creates a scaled Typography based on the given font scale factor.
 * @param scale The font scale factor (0.85f for Small, 1.0f for Normal, 1.15f for Large)
 * @return A Typography instance with scaled font sizes
 */
fun createScaledTypography(scale: Float): Typography {
    return Typography(
        headlineLarge = TextStyle(
            fontFamily = FontFamily.Default,
            fontWeight = FontWeight.Bold,
            fontSize = scaledSp(24, scale, 20),
            lineHeight = scaledSp(32, scale, 28),
        ),
        headlineMedium = TextStyle(
            fontFamily = FontFamily.Default,
            fontWeight = FontWeight.SemiBold,
            fontSize = scaledSp(20, scale, 16),
            lineHeight = scaledSp(28, scale, 24),
        ),
        titleLarge = TextStyle(
            fontFamily = FontFamily.Default,
            fontWeight = FontWeight.SemiBold,
            fontSize = scaledSp(18, scale, 14),
            lineHeight = scaledSp(24, scale, 20),
        ),
        titleMedium = TextStyle(
            fontFamily = FontFamily.Default,
            fontWeight = FontWeight.Medium,
            fontSize = scaledSp(16, scale, 12),
            lineHeight = scaledSp(22, scale, 18),
        ),
        titleSmall = TextStyle(
            fontFamily = FontFamily.Default,
            fontWeight = FontWeight.Medium,
            fontSize = scaledSp(14, scale, 12),
            lineHeight = scaledSp(20, scale, 16),
        ),
        bodyLarge = TextStyle(
            fontFamily = FontFamily.Default,
            fontWeight = FontWeight.Normal,
            fontSize = scaledSp(16, scale, 14),
            lineHeight = scaledSp(24, scale, 20),
            letterSpacing = 0.15.sp,
        ),
        bodyMedium = TextStyle(
            fontFamily = FontFamily.Default,
            fontWeight = FontWeight.Normal,
            fontSize = scaledSp(14, scale, 12),
            lineHeight = scaledSp(20, scale, 16),
            letterSpacing = 0.25.sp,
        ),
        bodySmall = TextStyle(
            fontFamily = FontFamily.Default,
            fontWeight = FontWeight.Normal,
            fontSize = scaledSp(12, scale, 10),
            lineHeight = scaledSp(16, scale, 14),
            letterSpacing = 0.4.sp,
        ),
        labelLarge = TextStyle(
            fontFamily = FontFamily.Default,
            fontWeight = FontWeight.Medium,
            fontSize = scaledSp(14, scale, 12),
            lineHeight = scaledSp(20, scale, 16),
            letterSpacing = 0.1.sp,
        ),
        labelMedium = TextStyle(
            fontFamily = FontFamily.Default,
            fontWeight = FontWeight.Medium,
            fontSize = scaledSp(12, scale, 10),
            lineHeight = scaledSp(16, scale, 14),
            letterSpacing = 0.5.sp,
        ),
        labelSmall = TextStyle(
            fontFamily = FontFamily.Default,
            fontWeight = FontWeight.Medium,
            fontSize = scaledSp(10, scale, 8),
            lineHeight = scaledSp(14, scale, 12),
            letterSpacing = 0.5.sp,
        ),
    )
}

val GrottoTypography = Typography(
    headlineLarge = TextStyle(
        fontFamily = FontFamily.Default,
        fontWeight = FontWeight.Bold,
        fontSize = 24.sp,
        lineHeight = 32.sp,
    ),
    headlineMedium = TextStyle(
        fontFamily = FontFamily.Default,
        fontWeight = FontWeight.SemiBold,
        fontSize = 20.sp,
        lineHeight = 28.sp,
    ),
    titleLarge = TextStyle(
        fontFamily = FontFamily.Default,
        fontWeight = FontWeight.SemiBold,
        fontSize = 18.sp,
        lineHeight = 24.sp,
    ),
    titleMedium = TextStyle(
        fontFamily = FontFamily.Default,
        fontWeight = FontWeight.Medium,
        fontSize = 16.sp,
        lineHeight = 22.sp,
    ),
    titleSmall = TextStyle(
        fontFamily = FontFamily.Default,
        fontWeight = FontWeight.Medium,
        fontSize = 14.sp,
        lineHeight = 20.sp,
    ),
    bodyLarge = TextStyle(
        fontFamily = FontFamily.Default,
        fontWeight = FontWeight.Normal,
        fontSize = 16.sp,
        lineHeight = 24.sp,
        letterSpacing = 0.15.sp,
    ),
    bodyMedium = TextStyle(
        fontFamily = FontFamily.Default,
        fontWeight = FontWeight.Normal,
        fontSize = 14.sp,
        lineHeight = 20.sp,
        letterSpacing = 0.25.sp,
    ),
    bodySmall = TextStyle(
        fontFamily = FontFamily.Default,
        fontWeight = FontWeight.Normal,
        fontSize = 12.sp,
        lineHeight = 16.sp,
        letterSpacing = 0.4.sp,
    ),
    labelLarge = TextStyle(
        fontFamily = FontFamily.Default,
        fontWeight = FontWeight.Medium,
        fontSize = 14.sp,
        lineHeight = 20.sp,
        letterSpacing = 0.1.sp,
    ),
    labelMedium = TextStyle(
        fontFamily = FontFamily.Default,
        fontWeight = FontWeight.Medium,
        fontSize = 12.sp,
        lineHeight = 16.sp,
        letterSpacing = 0.5.sp,
    ),
    labelSmall = TextStyle(
        fontFamily = FontFamily.Default,
        fontWeight = FontWeight.Medium,
        fontSize = 10.sp,
        lineHeight = 14.sp,
        letterSpacing = 0.5.sp,
    ),
)
