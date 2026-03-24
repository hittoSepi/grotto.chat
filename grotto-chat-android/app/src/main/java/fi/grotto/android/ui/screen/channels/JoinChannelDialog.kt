package fi.grotto.android.ui.screen.channels

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import fi.grotto.android.ui.theme.GrottoSpacing

/**
 * Data class representing an available/suggested channel from the server.
 */
data class AvailableChannel(
    val name: String,
    val topic: String? = null,
    val userCount: Int = 0,
)

/**
 * Dialog state for joining a channel.
 */
sealed class JoinDialogState {
    data object Idle : JoinDialogState()
    data object Joining : JoinDialogState()
    data class Error(val message: String) : JoinDialogState()
    data object Success : JoinDialogState()
}

/**
 * Sanitize a channel name according to IRC standards (mirrors server-side logic):
 * 1. Auto-add # prefix if missing (or any other channel prefix)
 * 2. Strip invalid characters
 * 3. Remove duplicate prefixes
 * 4. Trim whitespace
 *
 * Valid IRC channel name characters (after the prefix):
 * - Alphanumeric characters (a-z, A-Z, 0-9)
 * - Special characters: - _ [ \ ] { } ^ ` | ~
 */
fun sanitizeChannelName(input: String): String {
    if (input.isEmpty()) {
        return "#"
    }

    val result = StringBuilder(input.length + 1)
    var hasPrefix = false
    var prefixAdded = false

    for (i in input.indices) {
        val c = input[i]

        // Skip leading whitespace
        if (!prefixAdded && c.isWhitespace()) {
            continue
        }

        // Handle channel prefix (# & ! +)
        if (isChannelPrefix(c)) {
            if (!hasPrefix) {
                // First prefix character - use # as the canonical prefix
                result.append('#')
                hasPrefix = true
                prefixAdded = true
            }
            // Skip duplicate prefixes (e.g., ##test -> #test)
            continue
        }

        // Now we're past any potential prefix
        prefixAdded = true

        // Skip invalid characters (spaces, control chars, punctuation, etc.)
        if (!isValidChannelChar(c)) {
            continue
        }

        result.append(c)
    }

    // If no prefix was found, add # at the beginning
    if (!hasPrefix) {
        result.insert(0, '#')
    }

    // Handle edge case: result is just "#" (no valid characters)
    return if (result.toString() == "#") "#" else result.toString()
}

private fun isChannelPrefix(c: Char): Boolean {
    return c == '#' || c == '&' || c == '!' || c == '+'
}

private fun isValidChannelChar(c: Char): Boolean {
    // Alphanumeric
    if (c.isLetterOrDigit()) {
        return true
    }
    // Allowed special characters
    return when (c) {
        '-', '_', '[', ']', '{', '}', '^', '`', '|', '~' -> true
        else -> false
    }
}

/**
 * Check if a channel name is valid (has content after the prefix).
 */
fun isValidChannelName(channel: String): Boolean {
    if (channel.isEmpty()) {
        return false
    }
    // Must start with a valid prefix
    if (!isChannelPrefix(channel[0])) {
        return false
    }
    // Must have at least one character after the prefix
    return channel.length > 1
}

/**
 * Dialog for joining a channel.
 *
 * @param availableChannels Optional list of available/suggested channels from the server
 * @param dialogState Current state of the join operation
 * @param onJoin Called when user confirms with a channel name
 * @param onDismiss Called when dialog is dismissed
 */
@Composable
fun JoinChannelDialog(
    availableChannels: List<AvailableChannel> = emptyList(),
    dialogState: JoinDialogState = JoinDialogState.Idle,
    onJoin: (String) -> Unit,
    onDismiss: () -> Unit,
) {
    var channelInput by remember { mutableStateOf("") }
    var sanitizedInput by remember { mutableStateOf("") }
    val focusRequester = remember { FocusRequester() }

    // Auto-sanitize as user types (for display purposes)
    LaunchedEffect(channelInput) {
        sanitizedInput = sanitizeChannelName(channelInput)
    }

    AlertDialog(
        onDismissRequest = { 
            if (dialogState !is JoinDialogState.Joining) {
                onDismiss()
            }
        },
        title = { Text("Join Channel") },
        text = {
            Column(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(GrottoSpacing.md),
            ) {
                // Channel name input
                OutlinedTextField(
                    value = channelInput,
                    onValueChange = { channelInput = it },
                    modifier = Modifier
                        .fillMaxWidth()
                        .focusRequester(focusRequester),
                    label = { Text("Channel name") },
                    placeholder = { Text("#general") },
                    singleLine = true,
                    enabled = dialogState !is JoinDialogState.Joining,
                    keyboardOptions = KeyboardOptions(
                        keyboardType = KeyboardType.Text,
                        imeAction = ImeAction.Done,
                    ),
                    keyboardActions = KeyboardActions(
                        onDone = {
                            if (sanitizedInput.length > 1 && dialogState !is JoinDialogState.Joining) {
                                onJoin(sanitizedInput)
                            }
                        }
                    ),
                    supportingText = {
                        if (channelInput.isNotEmpty() && sanitizedInput != channelInput) {
                            Text(
                                text = "Will join: $sanitizedInput",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.primary,
                            )
                        }
                    },
                    isError = dialogState is JoinDialogState.Error,
                )

                // Error message
                if (dialogState is JoinDialogState.Error) {
                    Text(
                        text = (dialogState as JoinDialogState.Error).message,
                        color = MaterialTheme.colorScheme.error,
                        style = MaterialTheme.typography.bodySmall,
                        modifier = Modifier.padding(start = GrottoSpacing.sm),
                    )
                }

                // Loading indicator
                if (dialogState is JoinDialogState.Joining) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.Center,
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        CircularProgressIndicator(
                            modifier = Modifier.width(24.dp),
                            strokeWidth = 2.dp,
                        )
                        Spacer(modifier = Modifier.width(GrottoSpacing.sm))
                        Text(
                            text = "Joining...",
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }

                // Available channels list (if provided by server)
                if (availableChannels.isNotEmpty() && dialogState !is JoinDialogState.Joining) {
                    Spacer(modifier = Modifier.height(GrottoSpacing.sm))
                    Text(
                        text = "Available channels",
                        style = MaterialTheme.typography.labelMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Spacer(modifier = Modifier.height(GrottoSpacing.xs))
                    
                    LazyColumn(
                        modifier = Modifier
                            .fillMaxWidth()
                            .heightIn(max = 150.dp),
                        verticalArrangement = Arrangement.spacedBy(GrottoSpacing.xs),
                    ) {
                        items(availableChannels) { channel ->
                            AvailableChannelItem(
                                channel = channel,
                                onClick = { onJoin(channel.name) },
                            )
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(
                onClick = { onJoin(sanitizedInput) },
                enabled = sanitizedInput.length > 1 && 
                    dialogState !is JoinDialogState.Joining,
            ) {
                Text("Join")
            }
        },
        dismissButton = {
            TextButton(
                onClick = onDismiss,
                enabled = dialogState !is JoinDialogState.Joining,
            ) {
                Text("Cancel")
            }
        },
    )

    // Request focus on the text field when dialog opens
    LaunchedEffect(Unit) {
        focusRequester.requestFocus()
    }
}

@Composable
private fun AvailableChannelItem(
    channel: AvailableChannel,
    onClick: () -> Unit,
) {
    Surface(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth(),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f),
        shape = MaterialTheme.shapes.small,
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = GrottoSpacing.sm, vertical = GrottoSpacing.xs),
        ) {
            Text(
                text = channel.name,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurface,
            )
            if (!channel.topic.isNullOrEmpty()) {
                Text(
                    text = channel.topic,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1,
                )
            }
        }
    }
}
