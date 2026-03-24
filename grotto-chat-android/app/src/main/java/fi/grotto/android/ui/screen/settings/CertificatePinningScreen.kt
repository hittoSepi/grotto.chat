package fi.grotto.android.ui.screen.settings

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
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Divider
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
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
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import fi.grotto.android.security.pinning.CertificatePin
import fi.grotto.android.ui.theme.GrottoSpacing

/**
 * Screen for managing certificate pinning settings.
 * 
 * Features:
 * - Enable/disable certificate pinning
 * - View existing pins
 * - Add new pins
 * - Delete pins
 * - Trust on first use (TOFU) settings
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun CertificatePinningScreen(
    onNavigateBack: () -> Unit,
    viewModel: CertificatePinningViewModel = hiltViewModel(),
) {
    val uiState by viewModel.uiState.collectAsState()
    var showAddDialog by remember { mutableStateOf(false) }
    var pinToDelete by remember { mutableStateOf<CertificatePin?>(null) }
    
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Certificate Pinning") },
                navigationIcon = {
                    IconButton(onClick = onNavigateBack) {
                        Icon(Icons.Default.ArrowBack, contentDescription = "Back")
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surface,
                ),
            )
        },
        floatingActionButton = {
            FloatingActionButton(onClick = { showAddDialog = true }) {
                Icon(Icons.Default.Add, contentDescription = "Add pin")
            }
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .verticalScroll(rememberScrollState())
                .padding(GrottoSpacing.screenPadding),
        ) {
            // Status card
            PinningStatusCard(
                isEnabled = uiState.isPinningEnabled,
                pinCount = uiState.pins.size,
                onToggle = viewModel::setPinningEnabled
            )
            
            Spacer(modifier = Modifier.height(GrottoSpacing.medium))
            
            // Info card
            InfoCard()
            
            Spacer(modifier = Modifier.height(GrottoSpacing.medium))
            
            // Pins list
            Text(
                text = "Certificate Pins",
                style = MaterialTheme.typography.titleMedium,
            )
            
            Spacer(modifier = Modifier.height(GrottoSpacing.small))
            
            if (uiState.pins.isEmpty()) {
                EmptyPinsCard()
            } else {
                uiState.pins.forEach { pin ->
                    PinCard(
                        pin = pin,
                        onDelete = { pinToDelete = pin }
                    )
                    Spacer(modifier = Modifier.height(GrottoSpacing.small))
                }
            }
        }
    }
    
    // Add pin dialog
    if (showAddDialog) {
        AddPinDialog(
            onDismiss = { showAddDialog = false },
            onConfirm = { hostname, pin, isBackup ->
                viewModel.addPin(hostname, pin, isBackup)
                showAddDialog = false
            }
        )
    }
    
    // Delete confirmation dialog
    if (pinToDelete != null) {
        AlertDialog(
            onDismissRequest = { pinToDelete = null },
            title = { Text("Delete Pin") },
            text = { Text("Are you sure you want to delete this certificate pin for ${pinToDelete?.pattern}?") },
            confirmButton = {
                TextButton(
                    onClick = {
                        pinToDelete?.let { viewModel.removePin(it) }
                        pinToDelete = null
                    }
                ) {
                    Text("Delete")
                }
            },
            dismissButton = {
                TextButton(onClick = { pinToDelete = null }) {
                    Text("Cancel")
                }
            }
        )
    }
}

@Composable
private fun PinningStatusCard(
    isEnabled: Boolean,
    pinCount: Int,
    onToggle: (Boolean) -> Unit,
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(
            containerColor = if (isEnabled) 
                MaterialTheme.colorScheme.primaryContainer 
            else 
                MaterialTheme.colorScheme.surfaceVariant,
        ),
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(GrottoSpacing.medium),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                imageVector = if (isEnabled) Icons.Default.Lock else Icons.Default.Warning,
                contentDescription = null,
                tint = if (isEnabled) 
                    MaterialTheme.colorScheme.onPrimaryContainer 
                else 
                    MaterialTheme.colorScheme.onSurfaceVariant,
            )
            
            Spacer(modifier = Modifier.width(GrottoSpacing.medium))
            
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = if (isEnabled) "Pinning Enabled" else "Pinning Disabled",
                    style = MaterialTheme.typography.titleSmall,
                )
                Text(
                    text = "$pinCount pin${if (pinCount != 1) "s" else ""} configured",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
            
            Switch(
                checked = isEnabled,
                onCheckedChange = onToggle,
            )
        }
    }
}

@Composable
private fun InfoCard() {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceVariant,
        ),
    ) {
        Column(
            modifier = Modifier.padding(GrottoSpacing.medium),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    imageVector = Icons.Default.Security,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.primary,
                )
                Spacer(modifier = Modifier.width(GrottoSpacing.small))
                Text(
                    text = "About Certificate Pinning",
                    style = MaterialTheme.typography.titleSmall,
                )
            }
            
            Spacer(modifier = Modifier.height(GrottoSpacing.small))
            
            Text(
                text = "Certificate pinning ensures your app only connects to servers " +
                       "with specific certificates. This prevents MITM attacks.",
                style = MaterialTheme.typography.bodySmall,
            )
            
            Spacer(modifier = Modifier.height(GrottoSpacing.small))
            
            Text(
                text = "The pin is a SHA-256 hash of the server's public key. " +
                       "You can get this from your server administrator or extract it " +
                       "from the server's certificate.",
                style = MaterialTheme.typography.bodySmall,
            )
        }
    }
}

@Composable
private fun EmptyPinsCard() {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surface,
        ),
    ) {
        Text(
            text = "No certificate pins configured.\nTap + to add a pin.",
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier
                .padding(GrottoSpacing.large)
                .fillMaxWidth(),
        )
    }
}

@Composable
private fun PinCard(
    pin: CertificatePin,
    onDelete: () -> Unit,
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
                imageVector = if (pin.isBackupPin) Icons.Default.Warning else Icons.Default.Lock,
                contentDescription = null,
                tint = if (pin.isBackupPin) 
                    MaterialTheme.colorScheme.tertiary 
                else 
                    MaterialTheme.colorScheme.primary,
            )
            
            Spacer(modifier = Modifier.width(GrottoSpacing.medium))
            
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = pin.pattern,
                    style = MaterialTheme.typography.titleSmall,
                )
                Text(
                    text = pin.pin.take(16) + "...",
                    style = MaterialTheme.typography.bodySmall,
                    fontFamily = FontFamily.Monospace,
                )
                if (pin.isBackupPin) {
                    Text(
                        text = "Backup pin",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.tertiary,
                    )
                }
            }
            
            IconButton(onClick = onDelete) {
                Icon(Icons.Default.Delete, contentDescription = "Delete")
            }
        }
    }
}

@Composable
private fun AddPinDialog(
    onDismiss: () -> Unit,
    onConfirm: (hostname: String, pin: String, isBackup: Boolean) -> Unit,
) {
    var hostname by remember { mutableStateOf("") }
    var pin by remember { mutableStateOf("") }
    var isBackup by remember { mutableStateOf(false) }
    
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Add Certificate Pin") },
        text = {
            Column {
                OutlinedTextField(
                    value = hostname,
                    onValueChange = { hostname = it },
                    label = { Text("Hostname pattern") },
                    placeholder = { Text("*.example.com") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                
                Spacer(modifier = Modifier.height(GrottoSpacing.medium))
                
                OutlinedTextField(
                    value = pin,
                    onValueChange = { pin = it },
                    label = { Text("SHA-256 Pin") },
                    placeholder = { Text("sha256/AAAA...") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                
                Spacer(modifier = Modifier.height(GrottoSpacing.small))
                
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Switch(
                        checked = isBackup,
                        onCheckedChange = { isBackup = it },
                    )
                    Spacer(modifier = Modifier.width(GrottoSpacing.small))
                    Text("Backup pin")
                }
            }
        },
        confirmButton = {
            Button(
                onClick = { onConfirm(hostname, pin, isBackup) },
                enabled = hostname.isNotBlank() && pin.isNotBlank()
            ) {
                Text("Add")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel")
            }
        }
    )
}
