package fi.grotto.android.ui.screen.chat.components

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Link
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import fi.grotto.android.ui.theme.GrottoSpacing
import fi.grotto.android.ui.theme.GrottoTheme

@Composable
fun LinkPreviewCard(
    title: String?,
    description: String?,
    url: String,
    imageUrl: String? = null,
    siteName: String? = null,
    modifier: Modifier = Modifier,
) {
    val semantic = GrottoTheme.semanticColors
    val displayTitle = title?.takeIf { it.isNotBlank() } ?: siteName ?: extractDomain(url)
    val displayDescription = description?.takeIf { it.isNotBlank() }

    Surface(
        modifier = modifier.widthIn(max = GrottoSpacing.previewCardMaxWidth),
        shape = MaterialTheme.shapes.small,
        border = BorderStroke(1.dp, semantic.previewBorder),
        color = MaterialTheme.colorScheme.surfaceVariant,
        onClick = { /* URL opening handled by parent */ }
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            // Image placeholder (could be replaced with AsyncImage if Coil is added)
            if (imageUrl != null) {
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(120.dp),
                    color = MaterialTheme.colorScheme.surface
                ) {
                    // Placeholder for image - in production, use Coil AsyncImage
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(
                            Icons.Default.Link,
                            contentDescription = null,
                            modifier = Modifier.size(48.dp),
                            tint = MaterialTheme.colorScheme.outline
                        )
                    }
                }
            }

            Column(modifier = Modifier.padding(GrottoSpacing.previewCardPadding)) {
                // Title row with icon
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(
                        Icons.Default.Link,
                        contentDescription = null,
                        modifier = Modifier.size(16.dp),
                        tint = semantic.previewTitle,
                    )
                    Spacer(Modifier.width(GrottoSpacing.xs))
                    Text(
                        text = displayTitle,
                        style = MaterialTheme.typography.titleSmall,
                        color = semantic.previewTitle,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f)
                    )
                }

                // Description
                if (displayDescription != null) {
                    Text(
                        text = displayDescription,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        maxLines = 2,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.padding(top = GrottoSpacing.xs),
                    )
                }

                // URL/domain at bottom
                Text(
                    text = extractDomain(url),
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.outline,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.padding(top = GrottoSpacing.xs),
                )
            }
        }
    }
}

private fun extractDomain(url: String): String {
    return try {
        val uri = java.net.URI(url)
        uri.host?.removePrefix("www.") ?: url
    } catch (e: Exception) {
        url
    }
}
