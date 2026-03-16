package fi.grotto.android.ui.screen.notifications

import android.Manifest
import android.os.Build
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
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.NotificationsActive
import androidx.compose.material.icons.filled.NotificationsOff
import androidx.compose.material.icons.filled.Phone
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Divider
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import com.google.accompanist.permissions.ExperimentalPermissionsApi
import com.google.accompanist.permissions.isGranted
import com.google.accompanist.permissions.rememberPermissionState
import fi.grotto.android.ui.theme.GrottoSpacing

/**
 * Notification settings screen for managing push notification preferences.
 * 
 * Features:
 * - Enable/disable all notifications
 * - Configure mention notifications
 * - Configure call notifications
 * - Request notification permission (Android 13+)
 */
@OptIn(ExperimentalMaterial3Api::class, ExperimentalPermissionsApi::class)
@Composable
fun NotificationSettingsScreen(
    onNavigateBack: () -> Unit,
    viewModel: NotificationSettingsViewModel = hiltViewModel(),
) {
    val context = LocalContext.current
    val uiState by viewModel.uiState.collectAsState()
    
    // Notification permission for Android 13+
    val notificationPermissionState = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        rememberPermissionState(Manifest.permission.POST_NOTIFICATIONS)
    } else {
        null
    }
    
    var showPermissionRationale by remember { mutableStateOf(false) }
    
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Notifications") },
                navigationIcon = {
                    IconButton(onClick = onNavigateBack) {
                        Icon(Icons.Default.ArrowBack, contentDescription = "Back")
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surface,
                ),
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .verticalScroll(rememberScrollState())
                .padding(GrottoSpacing.screenPadding),
        ) {
            // Permission Card (Android 13+)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                NotificationPermissionCard(
                    hasPermission = notificationPermissionState?.status?.isGranted == true,
                    onRequestPermission = {
                        if (notificationPermissionState?.status?.isGranted == false) {
                            // TODO: Check shouldShowRationale when API is available
                            notificationPermissionState.launchPermissionRequest()
                        }
                    }
                )
                
                Spacer(modifier = Modifier.height(GrottoSpacing.medium))
            }
            
            // Main toggle
            NotificationToggleCard(
                title = "Enable Notifications",
                subtitle = "Receive push notifications for messages and calls",
                icon = if (uiState.notificationsEnabled) 
                    Icons.Default.Notifications else Icons.Default.NotificationsOff,
                checked = uiState.notificationsEnabled,
                onCheckedChange = { viewModel.setNotificationsEnabled(it) },
                enabled = notificationPermissionState?.status?.isGranted != false
            )
            
            Spacer(modifier = Modifier.height(GrottoSpacing.medium))
            
            if (uiState.notificationsEnabled) {
                // Mention notifications
                NotificationToggleCard(
                    title = "Mentions",
                    subtitle = "Notify when someone mentions you (@username)",
                    icon = Icons.Default.NotificationsActive,
                    checked = uiState.mentionNotificationsEnabled,
                    onCheckedChange = { viewModel.setMentionNotificationsEnabled(it) },
                )
                
                Spacer(modifier = Modifier.height(GrottoSpacing.medium))
                
                // Call notifications
                NotificationToggleCard(
                    title = "Voice Calls",
                    subtitle = "Notify for incoming voice calls",
                    icon = Icons.Default.Phone,
                    checked = uiState.callNotificationsEnabled,
                    onCheckedChange = { viewModel.setCallNotificationsEnabled(it) },
                )
                
                Spacer(modifier = Modifier.height(GrottoSpacing.medium))
            }
            
            // Info card
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surfaceVariant,
                ),
            ) {
                Column(
                    modifier = Modifier.padding(GrottoSpacing.medium),
                ) {
                    Text(
                        text = "About Notifications",
                        style = MaterialTheme.typography.titleSmall,
                    )
                    Spacer(modifier = Modifier.height(GrottoSpacing.small))
                    Text(
                        text = "For privacy, notification previews only show sender and channel names, " +
                               "not message content. The full message is decrypted when you open the app.",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }
    }
    
    // Permission rationale dialog
    if (showPermissionRationale) {
        AlertDialog(
            onDismissRequest = { showPermissionRationale = false },
            title = { Text("Notification Permission") },
            text = { Text("Notifications are needed to alert you about new messages and calls even when the app is in the background.") },
            confirmButton = {
                Button(
                    onClick = {
                        showPermissionRationale = false
                        notificationPermissionState?.launchPermissionRequest()
                    }
                ) {
                    Text("Grant Permission")
                }
            },
            dismissButton = {
                TextButton(
                    onClick = { showPermissionRationale = false }
                ) {
                    Text("Cancel")
                }
            }
        )
    }
}

@Composable
private fun NotificationPermissionCard(
    hasPermission: Boolean,
    onRequestPermission: () -> Unit,
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(
            containerColor = if (hasPermission) 
                MaterialTheme.colorScheme.primaryContainer 
            else 
                MaterialTheme.colorScheme.errorContainer,
        ),
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(GrottoSpacing.medium),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                imageVector = if (hasPermission) 
                    Icons.Default.Notifications else Icons.Default.NotificationsOff,
                contentDescription = null,
                tint = if (hasPermission) 
                    MaterialTheme.colorScheme.onPrimaryContainer 
                else 
                    MaterialTheme.colorScheme.onErrorContainer,
            )
            
            Spacer(modifier = Modifier.width(GrottoSpacing.medium))
            
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = if (hasPermission) "Permission Granted" else "Permission Required",
                    style = MaterialTheme.typography.titleSmall,
                )
                Text(
                    text = if (hasPermission) 
                        "You will receive push notifications" 
                    else 
                        "Enable notifications to receive alerts",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
            
            if (!hasPermission) {
                Button(onClick = onRequestPermission) {
                    Text("Enable")
                }
            }
        }
    }
}

@Composable
private fun NotificationToggleCard(
    title: String,
    subtitle: String,
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    enabled: Boolean = true,
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surface,
        ),
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(GrottoSpacing.medium),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                imageVector = icon,
                contentDescription = null,
                tint = if (enabled) 
                    MaterialTheme.colorScheme.primary 
                else 
                    MaterialTheme.colorScheme.onSurface.copy(alpha = 0.38f),
            )
            
            Spacer(modifier = Modifier.width(GrottoSpacing.medium))
            
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleSmall,
                    color = if (enabled) 
                        MaterialTheme.colorScheme.onSurface 
                    else 
                        MaterialTheme.colorScheme.onSurface.copy(alpha = 0.38f),
                )
                Text(
                    text = subtitle,
                    style = MaterialTheme.typography.bodySmall,
                    color = if (enabled) 
                        MaterialTheme.colorScheme.onSurfaceVariant 
                    else 
                        MaterialTheme.colorScheme.onSurface.copy(alpha = 0.38f),
                )
            }
            
            Switch(
                checked = checked,
                onCheckedChange = onCheckedChange,
                enabled = enabled,
            )
        }
    }
}
