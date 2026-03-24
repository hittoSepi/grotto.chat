package fi.grotto.android.ui.screen.chat.components

import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import fi.grotto.android.ui.theme.GrottoSpacing

@Composable
fun TypingIndicator(
    users: List<String>,
    modifier: Modifier = Modifier,
) {
    if (users.isEmpty()) return

    val transition = rememberInfiniteTransition(label = "typing")
    val alpha by transition.animateFloat(
        initialValue = 0.3f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(600), RepeatMode.Reverse),
        label = "dot",
    )

    Row(
        modifier = modifier.padding(horizontal = GrottoSpacing.messagePaddingHorizontal, vertical = GrottoSpacing.xs),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Row(horizontalArrangement = Arrangement.spacedBy(3.dp)) {
            repeat(3) {
                Box(
                    modifier = Modifier
                        .size(6.dp)
                        .clip(CircleShape)
                        .background(MaterialTheme.colorScheme.onSurfaceVariant)
                        .alpha(alpha),
                )
            }
        }
        Spacer(Modifier.width(GrottoSpacing.sm))
        val text = when {
            users.size == 1 -> "${users[0]} is typing..."
            users.size == 2 -> "${users[0]} and ${users[1]} are typing..."
            else -> "${users[0]} and ${users.size - 1} others are typing..."
        }
        Text(text, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}
