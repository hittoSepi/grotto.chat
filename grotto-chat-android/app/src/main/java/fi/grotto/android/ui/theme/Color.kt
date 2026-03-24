package fi.grotto.android.ui.theme

import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Immutable
import androidx.compose.ui.graphics.Color

// -- Dark Theme (Tokyo Night) --
val DarkColorScheme = darkColorScheme(
    primary = Color(0xFF7AA2F7),
    onPrimary = Color(0xFF1A1B26),
    primaryContainer = Color(0xFF2A3A6B),
    onPrimaryContainer = Color(0xFFBBD6FF),

    secondary = Color(0xFF9ECE6A),
    onSecondary = Color(0xFF1A1B26),
    secondaryContainer = Color(0xFF2D4A1E),
    onSecondaryContainer = Color(0xFFD4F5A0),

    tertiary = Color(0xFFBB9AF7),
    onTertiary = Color(0xFF1A1B26),
    tertiaryContainer = Color(0xFF3D2D5C),
    onTertiaryContainer = Color(0xFFE3D0FF),

    error = Color(0xFFF7768E),
    onError = Color(0xFF1A1B26),
    errorContainer = Color(0xFF5C1D28),
    onErrorContainer = Color(0xFFFFB3C0),

    background = Color(0xFF1A1B26),
    onBackground = Color(0xFFC0CAF5),
    surface = Color(0xFF24283B),
    onSurface = Color(0xFFC0CAF5),
    surfaceVariant = Color(0xFF2F3549),
    onSurfaceVariant = Color(0xFFA9B1D6),

    outline = Color(0xFF3B4261),
    outlineVariant = Color(0xFF2F3549),

    inverseSurface = Color(0xFFC0CAF5),
    inverseOnSurface = Color(0xFF1A1B26),
    inversePrimary = Color(0xFF3D5CC0),
)

// -- Light Theme --
val LightColorScheme = lightColorScheme(
    primary = Color(0xFF3D5CC0),
    onPrimary = Color(0xFFFFFFFF),
    primaryContainer = Color(0xFFDBE1FF),
    onPrimaryContainer = Color(0xFF001849),

    secondary = Color(0xFF4D7A2A),
    onSecondary = Color(0xFFFFFFFF),
    secondaryContainer = Color(0xFFCEF4A0),
    onSecondaryContainer = Color(0xFF0F2000),

    background = Color(0xFFF5F5F8),
    onBackground = Color(0xFF1A1B26),
    surface = Color(0xFFFFFFFF),
    onSurface = Color(0xFF1A1B26),

    error = Color(0xFFBA1A1A),
    outline = Color(0xFF757889),
)

// -- Semantic Colors --
@Immutable
data class GrottoSemanticColors(
    val timestamp: Color,
    val nickPalette: List<Color>,
    val systemMessage: Color,
    val encryptionOk: Color,
    val encryptionUnverified: Color,
    val encryptionWarning: Color,
    val voiceActive: Color,
    val voiceMuted: Color,
    val voiceSpeaking: Color,
    val unreadBadge: Color,
    val mentionBadge: Color,
    val previewBorder: Color,
    val previewTitle: Color,
    val statusOnline: Color,
    val statusAway: Color,
    val statusOffline: Color,
    val inputBackground: Color,
    val inputPlaceholder: Color,
)

val DarkGrottoColors = GrottoSemanticColors(
    timestamp = Color(0xFF565F89),
    nickPalette = listOf(
        Color(0xFF7AA2F7),
        Color(0xFF9ECE6A),
        Color(0xFFE0AF68),
        Color(0xFFBB9AF7),
        Color(0xFF7DCFFF),
        Color(0xFFF7768E),
        Color(0xFF73DACA),
        Color(0xFFFF9E64),
    ),
    systemMessage = Color(0xFFE0AF68),
    encryptionOk = Color(0xFF9ECE6A),
    encryptionUnverified = Color(0xFFE0AF68),
    encryptionWarning = Color(0xFFF7768E),
    voiceActive = Color(0xFF9ECE6A),
    voiceMuted = Color(0xFFF7768E),
    voiceSpeaking = Color(0xFF73DACA),
    unreadBadge = Color(0xFFF7768E),
    mentionBadge = Color(0xFFFF9E64),
    previewBorder = Color(0xFF3B4261),
    previewTitle = Color(0xFF7AA2F7),
    statusOnline = Color(0xFF9ECE6A),
    statusAway = Color(0xFFE0AF68),
    statusOffline = Color(0xFF565F89),
    inputBackground = Color(0xFF16161E),
    inputPlaceholder = Color(0xFF565F89),
)

val LightGrottoColors = GrottoSemanticColors(
    timestamp = Color(0xFF757889),
    nickPalette = listOf(
        Color(0xFF3D5CC0),
        Color(0xFF4D7A2A),
        Color(0xFFB8860B),
        Color(0xFF7B5EA7),
        Color(0xFF2980B9),
        Color(0xFFBA1A1A),
        Color(0xFF2E8B57),
        Color(0xFFD2691E),
    ),
    systemMessage = Color(0xFFB8860B),
    encryptionOk = Color(0xFF4D7A2A),
    encryptionUnverified = Color(0xFFB8860B),
    encryptionWarning = Color(0xFFBA1A1A),
    voiceActive = Color(0xFF4D7A2A),
    voiceMuted = Color(0xFFBA1A1A),
    voiceSpeaking = Color(0xFF2E8B57),
    unreadBadge = Color(0xFFBA1A1A),
    mentionBadge = Color(0xFFD2691E),
    previewBorder = Color(0xFFCCCCCC),
    previewTitle = Color(0xFF3D5CC0),
    statusOnline = Color(0xFF4D7A2A),
    statusAway = Color(0xFFB8860B),
    statusOffline = Color(0xFF757889),
    inputBackground = Color(0xFFEEEEEE),
    inputPlaceholder = Color(0xFF757889),
)

fun nickColor(nick: String, palette: List<Color>): Color {
    val hash = nick.hashCode().toUInt()
    return palette[(hash % palette.size.toUInt()).toInt()]
}
