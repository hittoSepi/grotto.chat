package fi.grotto.android.ui.screen.verify

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextAlign
import androidx.hilt.navigation.compose.hiltViewModel
import fi.grotto.android.ui.theme.GrottoSpacing
import fi.grotto.android.ui.theme.MonoStyle

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SafetyNumberScreen(
    peerId: String,
    onBack: () -> Unit,
    viewModel: SafetyNumberViewModel = hiltViewModel(),
) {
    val state by viewModel.uiState.collectAsState()
    
    LaunchedEffect(peerId) {
        viewModel.loadSafetyNumber(peerId)
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Verify: You \u2194 $peerId") },
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
                .padding(GrottoSpacing.xl),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Text(
                text = "If the numbers below match the ones on ${peerId}'s device, your connection is secure.",
                style = MaterialTheme.typography.bodyMedium,
                textAlign = TextAlign.Center,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )

            Spacer(Modifier.height(GrottoSpacing.xl))

            if (state.isLoading) {
                CircularProgressIndicator()
                Spacer(Modifier.height(GrottoSpacing.lg))
                Text("Computing safety number...", style = MaterialTheme.typography.bodyMedium)
            } else {
                Surface(
                    color = MaterialTheme.colorScheme.surfaceVariant,
                    shape = MaterialTheme.shapes.medium,
                ) {
                    Text(
                        text = state.safetyNumber,
                        style = MonoStyle,
                        textAlign = TextAlign.Center,
                        modifier = Modifier.padding(GrottoSpacing.xl),
                    )
                }

                Spacer(Modifier.height(GrottoSpacing.lg))

                Text(
                    text = "Compare this number with $peerId in person or via a trusted call.",
                    style = MaterialTheme.typography.bodySmall,
                    textAlign = TextAlign.Center,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )

                Spacer(Modifier.height(GrottoSpacing.xxl))

                if (state.isVerified) {
                    Button(
                        onClick = { /* Already verified */ },
                        modifier = Modifier.fillMaxWidth(),
                        enabled = false,
                    ) {
                        Text("\u2713 Verified")
                    }
                } else {
                    Button(
                        onClick = { viewModel.markAsVerified(peerId) },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Mark as Verified")
                    }
                }

                Spacer(Modifier.height(GrottoSpacing.md))

                OutlinedButton(
                    onClick = { /* TODO: copy to clipboard */ },
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Text("Copy to Clipboard")
                }

                Spacer(Modifier.height(GrottoSpacing.xl))

                val verifiedText = if (state.isVerified) {
                    "Identity verified"
                } else {
                    state.lastVerifiedAt?.let { "Last verified: ${formatTimestamp(it)}" } ?: "Last verified: never"
                }
                
                Text(
                    text = verifiedText,
                    style = MaterialTheme.typography.bodySmall,
                    color = if (state.isVerified) 
                        MaterialTheme.colorScheme.primary 
                    else 
                        MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

private fun formatTimestamp(timestamp: Long): String {
    val formatter = java.text.SimpleDateFormat("MMM dd, yyyy", java.util.Locale.getDefault())
    return formatter.format(java.util.Date(timestamp))
}
