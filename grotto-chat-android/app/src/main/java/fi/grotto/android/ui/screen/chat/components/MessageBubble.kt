package fi.grotto.android.ui.screen.chat.components

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import fi.grotto.android.domain.model.Message
import fi.grotto.android.domain.model.MessageType
import fi.grotto.android.domain.model.SendStatus
import fi.grotto.android.ui.theme.GrottoSpacing
import fi.grotto.android.ui.theme.GrottoTheme
import fi.grotto.android.ui.theme.nickColor
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun MessageBubble(
    message: Message,
    modifier: Modifier = Modifier,
    onRetry: ((Long) -> Unit)? = null,
    onDelete: ((Long) -> Unit)? = null,
) {
    val semantic = GrottoTheme.semanticColors
    val timeFormat = SimpleDateFormat("HH:mm", Locale.getDefault())
    val timeStr = timeFormat.format(Date(message.timestamp))
    val nickCol = nickColor(message.senderId, semantic.nickPalette)

    val isAction = message.type == MessageType.ACTION
    val isFailed = message.sendStatus == SendStatus.FAILED
    val isSending = message.sendStatus == SendStatus.SENDING

    Column(modifier = modifier) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                text = timeStr,
                style = MaterialTheme.typography.labelSmall,
                color = semantic.timestamp,
            )
            Spacer(Modifier.width(GrottoSpacing.sm))
            if (isAction) {
                // IRC action style: [time] * nick does something
                Text(
                    text = "* ${'$'}{message.senderId} ${'$'}{message.content}",
                    style = MaterialTheme.typography.bodyLarge,
                    fontStyle = FontStyle.Italic,
                    color = if (isFailed) MaterialTheme.colorScheme.error else nickCol,
                )
            } else {
                // Normal IRC-style: [time] nick  message
                Text(
                    text = message.senderId,
                    style = MaterialTheme.typography.titleSmall,
                    color = nickCol,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.width(GrottoSpacing.messageNickWidth),
                )
                Spacer(Modifier.width(GrottoSpacing.sm))
                MarkdownText(
                    text = message.content,
                    style = MaterialTheme.typography.bodyLarge,
                    color = if (isFailed) MaterialTheme.colorScheme.error
                            else if (isSending) MaterialTheme.colorScheme.onSurfaceVariant
                            else MaterialTheme.colorScheme.onSurface,
                )
            }

            // Failed message actions
            if (isFailed) {
                Spacer(Modifier.width(GrottoSpacing.xs))
                Text(
                    text = "Failed",
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.error,
                )
                if (onRetry != null) {
                    Icon(
                        imageVector = Icons.Default.Refresh,
                        contentDescription = "Retry",
                        tint = MaterialTheme.colorScheme.error,
                        modifier = Modifier
                            .size(16.dp)
                            .clickable { onRetry(message.id) },
                    )
                }
                if (onDelete != null) {
                    Icon(
                        imageVector = Icons.Default.Delete,
                        contentDescription = "Delete",
                        tint = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier
                            .size(16.dp)
                            .clickable { onDelete(message.id) },
                    )
                }
            }
        }

        // Link preview
        message.linkPreview?.let { preview ->
            LinkPreviewCard(
                title = preview.title,
                description = preview.description,
                url = preview.url,
                imageUrl = preview.imageUrl,
                modifier = Modifier.padding(
                    start = GrottoSpacing.previewCardMarginStart,
                    top = GrottoSpacing.xs,
                ),
            )
        }
    }
}
