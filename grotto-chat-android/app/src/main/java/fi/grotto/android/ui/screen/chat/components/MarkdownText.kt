package fi.grotto.android.ui.screen.chat.components

import androidx.compose.foundation.text.ClickableText
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.withStyle

/**
 * Renders message text with basic markdown:
 * **bold**, *italic*, `inline code`, ```code blocks```, and clickable URLs.
 */
@Composable
fun MarkdownText(
    text: String,
    modifier: Modifier = Modifier,
    style: TextStyle = MaterialTheme.typography.bodyLarge,
    color: Color = MaterialTheme.colorScheme.onSurface,
) {
    val uriHandler = LocalUriHandler.current
    val linkColor = MaterialTheme.colorScheme.primary
    val codeBackground = MaterialTheme.colorScheme.surfaceVariant
    val codeColor = MaterialTheme.colorScheme.onSurfaceVariant

    val annotated = parseMarkdown(text, color, linkColor, codeColor)

    ClickableText(
        text = annotated,
        style = style.copy(color = color),
        modifier = modifier,
        onClick = { offset ->
            annotated.getStringAnnotations("URL", offset, offset).firstOrNull()?.let {
                try {
                    uriHandler.openUri(it.item)
                } catch (_: Exception) { }
            }
        }
    )
}

private fun parseMarkdown(
    text: String,
    baseColor: Color,
    linkColor: Color,
    codeColor: Color,
): AnnotatedString = buildAnnotatedString {
    var i = 0
    val len = text.length

    while (i < len) {
        when {
            // Code block: ```...```
            text.startsWith("```", i) -> {
                val end = text.indexOf("```", i + 3)
                if (end != -1) {
                    val code = text.substring(i + 3, end).trimStart('\n')
                    withStyle(SpanStyle(fontFamily = FontFamily.Monospace, color = codeColor)) {
                        append(code)
                    }
                    i = end + 3
                } else {
                    append(text[i])
                    i++
                }
            }
            // Inline code: `...`
            text[i] == '`' -> {
                val end = text.indexOf('`', i + 1)
                if (end != -1) {
                    val code = text.substring(i + 1, end)
                    withStyle(SpanStyle(fontFamily = FontFamily.Monospace, color = codeColor)) {
                        append(code)
                    }
                    i = end + 1
                } else {
                    append(text[i])
                    i++
                }
            }
            // Bold: **...**
            text.startsWith("**", i) -> {
                val end = text.indexOf("**", i + 2)
                if (end != -1) {
                    withStyle(SpanStyle(fontWeight = FontWeight.Bold)) {
                        append(text.substring(i + 2, end))
                    }
                    i = end + 2
                } else {
                    append(text[i])
                    i++
                }
            }
            // Italic: *...*
            text[i] == '*' && (i + 1 < len) && text[i + 1] != '*' -> {
                val end = text.indexOf('*', i + 1)
                if (end != -1) {
                    withStyle(SpanStyle(fontStyle = FontStyle.Italic)) {
                        append(text.substring(i + 1, end))
                    }
                    i = end + 1
                } else {
                    append(text[i])
                    i++
                }
            }
            // URL: https://... or http://...
            text.startsWith("http://", i) || text.startsWith("https://", i) -> {
                val endIdx = findUrlEnd(text, i)
                val url = text.substring(i, endIdx)
                pushStringAnnotation("URL", url)
                withStyle(SpanStyle(color = linkColor, textDecoration = TextDecoration.Underline)) {
                    append(url)
                }
                pop()
                i = endIdx
            }
            else -> {
                append(text[i])
                i++
            }
        }
    }
}

private fun findUrlEnd(text: String, start: Int): Int {
    var i = start
    while (i < text.length && !text[i].isWhitespace()) {
        i++
    }
    // Strip trailing punctuation that's likely not part of the URL
    while (i > start && text[i - 1] in ",.)>;") {
        i--
    }
    return i
}
