package fi.grotto.android.ui.screen.chat.components

import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AttachFile
import androidx.compose.material.icons.filled.Send
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import fi.grotto.android.ui.theme.GrottoSpacing
import fi.grotto.android.ui.theme.GrottoTheme

@Composable
fun MessageInput(
    text: String,
    onTextChanged: (String) -> Unit,
    onSend: () -> Unit,
    onAttachFile: (Uri) -> Unit,
    enabled: Boolean,
    modifier: Modifier = Modifier,
) {
    val semantic = GrottoTheme.semanticColors
    
    // File picker launcher
    val filePickerLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        uri?.let { onAttachFile(it) }
    }

    Surface(
        color = semantic.inputBackground,
        modifier = modifier,
    ) {
        Row(
            modifier = Modifier
                .padding(GrottoSpacing.inputBarPadding)
                .fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextField(
                value = text,
                onValueChange = onTextChanged,
                enabled = true,
                modifier = Modifier
                    .weight(1f)
                    .heightIn(min = GrottoSpacing.inputBarHeight),
                placeholder = {
                    Text(
                        "Write a message...",
                        color = semantic.inputPlaceholder,
                    )
                },
                colors = TextFieldDefaults.colors(
                    focusedContainerColor = Color.Transparent,
                    unfocusedContainerColor = Color.Transparent,
                    disabledContainerColor = Color.Transparent,
                    focusedIndicatorColor = Color.Transparent,
                    unfocusedIndicatorColor = Color.Transparent,
                ),
                shape = MaterialTheme.shapes.extraLarge,
                singleLine = false,
                maxLines = 4,
            )

            IconButton(
                onClick = { filePickerLauncher.launch("*/*") },
                enabled = enabled
            ) {
                Icon(Icons.Default.AttachFile, contentDescription = "Attach file")
            }

            IconButton(
                onClick = onSend,
                enabled = enabled && text.isNotBlank(),
            ) {
                Icon(
                    Icons.Default.Send,
                    contentDescription = "Send",
                    tint = if (text.isNotBlank()) MaterialTheme.colorScheme.primary
                    else MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}
