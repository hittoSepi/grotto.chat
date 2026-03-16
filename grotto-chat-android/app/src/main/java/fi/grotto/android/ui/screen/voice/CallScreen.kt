package fi.grotto.android.ui.screen.voice

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Call
import androidx.compose.material.icons.filled.CallEnd
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import fi.grotto.android.ui.components.UserAvatar
import fi.grotto.android.ui.theme.GrottoSpacing
import fi.grotto.android.ui.theme.GrottoTheme

@Composable
fun CallScreen(
    peerId: String,
    onHangup: () -> Unit,
    viewModel: VoiceViewModel = hiltViewModel(),
) {
    val state by viewModel.uiState.collectAsState()
    val semantic = GrottoTheme.semanticColors

    Surface(modifier = Modifier.fillMaxSize()) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(GrottoSpacing.xl),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center,
        ) {
            UserAvatar(userId = peerId, size = 96.dp)

            Spacer(Modifier.height(GrottoSpacing.lg))

            Text(peerId, style = MaterialTheme.typography.headlineMedium)

            Spacer(Modifier.height(GrottoSpacing.sm))

            Text(
                if (state.isIncoming) "Incoming voice call" else "Voice call",
                style = MaterialTheme.typography.bodyLarge,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )

            Spacer(Modifier.height(GrottoSpacing.sm))

            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Default.Lock, null, tint = semantic.encryptionOk, modifier = Modifier.size(16.dp))
                Spacer(Modifier.width(GrottoSpacing.xs))
                Text("E2E Encrypted", style = MaterialTheme.typography.bodySmall, color = semantic.encryptionOk)
            }

            Spacer(Modifier.height(GrottoSpacing.xxxl))

            if (state.isIncoming) {
                Row(horizontalArrangement = Arrangement.spacedBy(GrottoSpacing.xxl)) {
                    Button(
                        onClick = {
                            viewModel.declineCall()
                            onHangup()
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error),
                    ) {
                        Icon(Icons.Default.CallEnd, null)
                        Spacer(Modifier.width(GrottoSpacing.sm))
                        Text("Decline")
                    }
                    Button(
                        onClick = viewModel::acceptCall,
                        colors = ButtonDefaults.buttonColors(containerColor = semantic.voiceActive),
                    ) {
                        Icon(Icons.Default.Call, null)
                        Spacer(Modifier.width(GrottoSpacing.sm))
                        Text("Accept")
                    }
                }
            } else {
                Button(
                    onClick = {
                        viewModel.leave()
                        onHangup()
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error),
                ) {
                    Icon(Icons.Default.CallEnd, null)
                    Spacer(Modifier.width(GrottoSpacing.sm))
                    Text("Hang up")
                }
            }
        }
    }
}
