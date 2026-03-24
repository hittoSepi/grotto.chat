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
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Fingerprint
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
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
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.fragment.app.FragmentActivity
import androidx.hilt.navigation.compose.hiltViewModel
import fi.grotto.android.security.biometric.BiometricAuthManager
import fi.grotto.android.security.biometric.BiometricAvailability
import fi.grotto.android.ui.theme.GrottoSpacing

/**
 * Screen for managing biometric authentication settings.
 * 
 * Features:
 * - Enable/disable biometric unlock for identity key
 * - Test biometric authentication
 * - Show availability status
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun BiometricSettingsScreen(
    onNavigateBack: () -> Unit,
    viewModel: BiometricSettingsViewModel = hiltViewModel(),
) {
    val context = LocalContext.current
    val uiState by viewModel.uiState.collectAsState()
    
    var showEnrollDialog by remember { mutableStateOf(false) }
    var showTestPrompt by remember { mutableStateOf(false) }
    
    // Check biometric availability
    val biometricManager = remember { BiometricAuthManager(context) }
    val availability = remember { biometricManager.canAuthenticate() }
    val isAvailable = availability is BiometricAvailability.Available
    
    // Handle enabling biometrics
    LaunchedEffect(uiState.showBiometricPrompt) {
        if (uiState.showBiometricPrompt) {
            val activity = context as? FragmentActivity
            if (activity != null) {
                val success = biometricManager.authenticate(
                    activity = activity,
                    title = "Confirm Biometrics",
                    subtitle = "Authenticate to enable biometric unlock"
                )
                viewModel.onBiometricPromptResult(success)
            } else {
                viewModel.onBiometricPromptResult(false)
            }
        }
    }
    
    // Handle test prompt
    LaunchedEffect(showTestPrompt) {
        if (showTestPrompt) {
            val activity = context as? FragmentActivity
            if (activity != null) {
                biometricManager.authenticate(
                    activity = activity,
                    title = "Test Biometrics",
                    subtitle = "Verify your biometric works"
                )
            }
            showTestPrompt = false
        }
    }
    
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Biometric Authentication") },
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
            // Status card
            BiometricStatusCard(availability)
            
            Spacer(modifier = Modifier.height(GrottoSpacing.medium))
            
            // Enable toggle
            if (isAvailable) {
                BiometricToggleCard(
                    enabled = uiState.isBiometricEnabled,
                    onToggle = { enabled ->
                        if (enabled) {
                            viewModel.requestEnableBiometric()
                        } else {
                            viewModel.setBiometricEnabled(false)
                        }
                    }
                )
                
                Spacer(modifier = Modifier.height(GrottoSpacing.medium))
                
                // Test button
                Button(
                    onClick = { showTestPrompt = true },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Icon(Icons.Default.Fingerprint, contentDescription = null)
                    Spacer(modifier = Modifier.width(GrottoSpacing.small))
                    Text("Test Biometric Authentication")
                }
                
                Spacer(modifier = Modifier.height(GrottoSpacing.medium))
            }
            
            // Info card
            BiometricInfoCard()
            
            Spacer(modifier = Modifier.height(GrottoSpacing.medium))
            
            // Security notes
            SecurityNotesCard()
        }
    }
    
    // Enroll dialog
    if (showEnrollDialog) {
        AlertDialog(
            onDismissRequest = { showEnrollDialog = false },
            title = { Text("Biometrics Not Set Up") },
            text = { Text("Please enroll a fingerprint or face recognition in your device settings first.") },
            confirmButton = {
                Button(onClick = { showEnrollDialog = false }) {
                    Text("OK")
                }
            }
        )
    }
}

@Composable
private fun BiometricStatusCard(availability: BiometricAvailability) {
    val (icon, title, subtitle, color) = when (availability) {
        is BiometricAvailability.Available ->
            Quadruple(Icons.Default.Fingerprint, "Biometrics Available", 
                "Your device supports biometric authentication", 
                MaterialTheme.colorScheme.primaryContainer)
        is BiometricAvailability.NotEnrolled ->
            Quadruple(Icons.Default.Warning, "Biometrics Not Set Up", 
                "Please enroll a fingerprint or face in device settings", 
                MaterialTheme.colorScheme.errorContainer)
        is BiometricAvailability.NoHardware,
        is BiometricAvailability.HardwareUnavailable ->
            Quadruple(Icons.Default.Lock, "Biometrics Not Available", 
                "This device doesn't support biometric authentication", 
                MaterialTheme.colorScheme.surfaceVariant)
        else ->
            Quadruple(Icons.Default.Warning, "Biometrics Status Unknown", 
                "Unable to determine biometric availability", 
                MaterialTheme.colorScheme.surfaceVariant)
    }
    
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = color),
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
                tint = MaterialTheme.colorScheme.onSurface,
            )
            Spacer(modifier = Modifier.width(GrottoSpacing.medium))
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleSmall,
                )
                Text(
                    text = subtitle,
                    style = MaterialTheme.typography.bodySmall,
                )
            }
        }
    }
}

@Composable
private fun BiometricToggleCard(
    enabled: Boolean,
    onToggle: (Boolean) -> Unit,
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
                imageVector = Icons.Default.Security,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.primary,
            )
            Spacer(modifier = Modifier.width(GrottoSpacing.medium))
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = "Unlock with Biometrics",
                    style = MaterialTheme.typography.titleSmall,
                )
                Text(
                    text = "Require fingerprint or face to access your identity key",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
            Switch(
                checked = enabled,
                onCheckedChange = onToggle,
            )
        }
    }
}

@Composable
private fun BiometricInfoCard() {
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
                text = "About Biometric Authentication",
                style = MaterialTheme.typography.titleSmall,
            )
            Spacer(modifier = Modifier.height(GrottoSpacing.small))
            Text(
                text = "When enabled, you'll need to authenticate with your fingerprint " +
                       "or face recognition before accessing your cryptographic identity key. " +
                       "This adds an extra layer of security to your encrypted messages.",
                style = MaterialTheme.typography.bodySmall,
            )
        }
    }
}

@Composable
private fun SecurityNotesCard() {
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
                text = "Security Notes",
                style = MaterialTheme.typography.titleSmall,
            )
            Spacer(modifier = Modifier.height(GrottoSpacing.small))
            SecurityNote("Your biometric data never leaves the device")
            SecurityNote("Stored in Android Keystore, not the app")
            SecurityNote("You can disable this at any time")
            SecurityNote("Fallback to passphrase is always available")
        }
    }
}

@Composable
private fun SecurityNote(text: String) {
    Row(
        modifier = Modifier.padding(vertical = GrottoSpacing.extraSmall),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = "•",
            style = MaterialTheme.typography.bodyMedium,
            modifier = Modifier.padding(end = GrottoSpacing.small),
        )
        Text(
            text = text,
            style = MaterialTheme.typography.bodySmall,
        )
    }
}

// Helper for quadruple return
data class Quadruple<A, B, C, D>(val first: A, val second: B, val third: C, val fourth: D)
