package fi.grotto.android.ui.screen.voice

import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CallEnd
import androidx.compose.material.icons.filled.HeadsetOff
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.MicOff
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import fi.grotto.android.ui.components.UserAvatar
import fi.grotto.android.ui.theme.GrottoSpacing
import fi.grotto.android.ui.theme.GrottoTheme

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun VoiceOverlay(
    onLeave: () -> Unit,
    viewModel: VoiceViewModel = hiltViewModel(),
) {
    val state by viewModel.uiState.collectAsState()
    val semantic = GrottoTheme.semanticColors

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Voice: ${state.channelName}") },
                actions = {
                    if (state.isEncrypted) {
                        Icon(Icons.Default.Lock, null, tint = semantic.encryptionOk)
                    }
                },
            )
        },
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(GrottoSpacing.xl),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            // Participants grid
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.Center,
            ) {
                state.participants.forEach { p ->
                    Column(
                        horizontalAlignment = Alignment.CenterHorizontally,
                        modifier = Modifier.padding(GrottoSpacing.md),
                    ) {
                        val borderColor = when {
                            p.isSpeaking -> semantic.voiceSpeaking
                            p.isMuted -> semantic.voiceMuted
                            else -> MaterialTheme.colorScheme.outline
                        }
                        Box(
                            modifier = Modifier
                                .size(72.dp)
                                .clip(CircleShape)
                                .border(3.dp, borderColor, CircleShape),
                            contentAlignment = Alignment.Center,
                        ) {
                            UserAvatar(userId = p.userId, size = 64.dp)
                            if (p.isMuted) {
                                Icon(
                                    Icons.Default.MicOff,
                                    null,
                                    tint = semantic.voiceMuted,
                                    modifier = Modifier
                                        .align(Alignment.BottomEnd)
                                        .size(20.dp),
                                )
                            }
                        }
                        Spacer(Modifier.height(GrottoSpacing.xs))
                        Text(p.userId, style = MaterialTheme.typography.bodySmall)
                        if (p.isSpeaking) {
                            Text("(speaking)", style = MaterialTheme.typography.labelSmall, color = semantic.voiceSpeaking)
                        } else if (p.isMuted) {
                            Text("(muted)", style = MaterialTheme.typography.labelSmall, color = semantic.voiceMuted)
                        }
                    }
                }
            }

            Spacer(Modifier.weight(1f))

            // Status info
            Text(
                "Connected \u00B7 E2E \u00B7 ${state.codec}",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Text(
                "Latency: ${state.latencyMs}ms",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )

            Spacer(Modifier.height(GrottoSpacing.xxl))

            // Controls
            Row(
                horizontalArrangement = Arrangement.spacedBy(GrottoSpacing.xl),
            ) {
                IconButton(onClick = viewModel::toggleMute) {
                    Icon(
                        Icons.Default.MicOff,
                        "Mute",
                        tint = if (state.isMuted) semantic.voiceMuted else MaterialTheme.colorScheme.onSurface,
                    )
                }
                IconButton(onClick = viewModel::toggleDeafen) {
                    Icon(
                        Icons.Default.HeadsetOff,
                        "Deafen",
                        tint = if (state.isDeafened) semantic.voiceMuted else MaterialTheme.colorScheme.onSurface,
                    )
                }
            }

            Spacer(Modifier.height(GrottoSpacing.lg))

            Button(
                onClick = {
                    viewModel.leave()
                    onLeave()
                },
                colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error),
            ) {
                Icon(Icons.Default.CallEnd, null)
                Spacer(Modifier.width(GrottoSpacing.sm))
                Text("Leave")
            }

            Spacer(Modifier.height(GrottoSpacing.xl))
        }
    }
}
