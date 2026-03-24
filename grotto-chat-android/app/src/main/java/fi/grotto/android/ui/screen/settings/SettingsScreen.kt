package fi.grotto.android.ui.screen.settings

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.hilt.navigation.compose.hiltViewModel
import fi.grotto.android.data.local.preferences.UserPreferences
import fi.grotto.android.ui.theme.GrottoSpacing
import fi.grotto.android.ui.theme.GrottoTheme

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    onBack: () -> Unit,
    onNavigateToBiometric: () -> Unit = {},
    onNavigateToCertificatePinning: () -> Unit = {},
    viewModel: SettingsViewModel = hiltViewModel(),
) {
    val state by viewModel.uiState.collectAsState()
    val semantic = GrottoTheme.semanticColors
    
    // Dialog states
    var showThemeDialog by remember { mutableStateOf(false) }
    var showFontSizeDialog by remember { mutableStateOf(false) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Settings") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, "Back")
                    }
                },
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .verticalScroll(rememberScrollState()),
        ) {
            SectionTitle("ACCOUNT")
            SettingsRow("Nickname", state.nickname)
            SettingsRow("Identity key", state.identityFingerprint)
            SettingsRow("Export key backup", "")

            SectionTitle("SERVER")
            SettingsRow("Address", state.serverAddress)
            SettingsRow("Port", state.port)
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = GrottoSpacing.lg, vertical = GrottoSpacing.md),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text("Status", modifier = Modifier.weight(1f))
                Text(
                    if (state.isConnected) "Connected" else "Disconnected",
                    color = if (state.isConnected) semantic.statusOnline else semantic.statusOffline,
                )
            }

            SectionTitle("APPEARANCE")
            // Theme selector - clickable
            val themeDisplayName = when (state.themeMode) {
                UserPreferences.THEME_SYSTEM -> "System default"
                UserPreferences.THEME_LIGHT -> "Light"
                UserPreferences.THEME_DARK -> "Dark"
                else -> "System default"
            }
            SettingsRow(
                label = "Theme",
                value = themeDisplayName,
                onClick = { showThemeDialog = true },
            )
            // Font size selector - clickable
            val fontScaleDisplayName = when (state.fontScale) {
                UserPreferences.FONT_SCALE_SMALL -> "Small"
                UserPreferences.FONT_SCALE_NORMAL -> "Normal"
                UserPreferences.FONT_SCALE_LARGE -> "Large"
                else -> "Normal"
            }
            SettingsRow(
                label = "Font size",
                value = fontScaleDisplayName,
                onClick = { showFontSizeDialog = true },
            )
            SettingsRow("Message style", state.messageStyle)
            SettingsRow("Timestamp", state.timestampFormat)
            SettingsToggle("Compact mode", state.compactMode, viewModel::setCompactMode)
            SettingsToggle("Link previews", state.linkPreviewsEnabled, viewModel::setLinkPreviewsEnabled)

            SectionTitle("NOTIFICATIONS")
            SettingsToggle("Mentions", state.notifyMentions, viewModel::setNotifyMentions)
            SettingsToggle("DMs", state.notifyDMs, viewModel::setNotifyDMs)
            SettingsToggle("Sound", state.notifySound, viewModel::setNotifySound)

            SectionTitle("VOICE")
            SettingsToggle("Push-to-talk", state.pushToTalk, viewModel::setPushToTalk)
            SettingsToggle("Noise suppression", state.noiseSuppression, viewModel::setNoiseSuppression)
            SettingsRow("Bitrate", state.voiceBitrate)

            SectionTitle("SECURITY")
            SettingsToggle("Screen capture", state.screenCapture, viewModel::setScreenCapture)
            SettingsRow("Biometric auth", "", onClick = onNavigateToBiometric)
            SettingsRow("Certificate pinning", "", onClick = onNavigateToCertificatePinning)
            SettingsRow("Verified peers", "")
            SettingsRow("Blocked users", "")

            SectionTitle("ABOUT")
            SettingsRow("Version", state.version)
            SettingsRow("Protocol", state.protocolVersion)
            SettingsRow("Licenses", "")

            Spacer(Modifier.height(GrottoSpacing.xxl))
        }
    }
    
    // Theme selector dialog
    if (showThemeDialog) {
        ThemeSelectorDialog(
            currentTheme = state.themeMode,
            onThemeSelected = { mode ->
                viewModel.setThemeMode(mode)
                showThemeDialog = false
            },
            onDismiss = { showThemeDialog = false },
        )
    }
    
    // Font size selector dialog
    if (showFontSizeDialog) {
        FontSizeSelectorDialog(
            currentFontScale = state.fontScale,
            onFontScaleSelected = { scale ->
                viewModel.setFontScale(scale)
                showFontSizeDialog = false
            },
            onDismiss = { showFontSizeDialog = false },
        )
    }
}

@Composable
private fun SectionTitle(title: String) {
    HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
    Text(
        text = title,
        style = MaterialTheme.typography.labelMedium,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        modifier = Modifier.padding(
            start = GrottoSpacing.lg,
            top = GrottoSpacing.lg,
            bottom = GrottoSpacing.xs,
        ),
    )
}

@Composable
private fun SettingsRow(
    label: String,
    value: String,
    onClick: (() -> Unit)? = null,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(enabled = onClick != null, onClick = onClick ?: {})
            .padding(horizontal = GrottoSpacing.lg, vertical = GrottoSpacing.md),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(label, modifier = Modifier.weight(1f))
        if (value.isNotEmpty()) {
            Text(value, color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.bodySmall)
            Spacer(Modifier.width(GrottoSpacing.xs))
        }
        Text(
            if (onClick != null) ">" else "",
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun SettingsToggle(label: String, checked: Boolean, onChanged: (Boolean) -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = GrottoSpacing.lg, vertical = GrottoSpacing.xs),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(label, modifier = Modifier.weight(1f))
        Switch(checked = checked, onCheckedChange = onChanged)
    }
}
