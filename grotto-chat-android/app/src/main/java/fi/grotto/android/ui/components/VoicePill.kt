package fi.grotto.android.ui.components

import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import fi.grotto.android.ui.theme.GrottoSpacing
import fi.grotto.android.ui.theme.GrottoTheme

@Composable
fun VoicePill(
    channelName: String,
    participantCount: Int,
    onJoin: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val semantic = GrottoTheme.semanticColors

    Surface(
        color = semantic.voiceActive.copy(alpha = 0.15f),
        modifier = modifier.height(GrottoSpacing.voicePillHeight),
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = GrottoSpacing.lg),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                Icons.Default.Mic,
                null,
                tint = semantic.voiceActive,
                modifier = Modifier.size(18.dp),
            )
            Spacer(Modifier.width(GrottoSpacing.sm))
            Text(
                "Voice: $channelName ($participantCount)",
                style = MaterialTheme.typography.labelLarge,
                modifier = Modifier.weight(1f),
            )
            TextButton(onClick = onJoin) {
                Text("Join")
            }
        }
    }
}
