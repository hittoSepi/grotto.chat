package fi.grotto.android.ui.screen.chat.components

import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AttachFile
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import fi.grotto.android.domain.model.FileType

/**
 * Button for attaching files to messages.
 */
@Composable
fun FileAttachmentButton(
    onFileSelected: (Uri) -> Unit,
    enabled: Boolean = true,
    modifier: Modifier = Modifier
) {
    // File picker launcher
    val filePickerLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        uri?.let { onFileSelected(it) }
    }
    
    IconButton(
        onClick = { filePickerLauncher.launch("*/*") },
        enabled = enabled,
        modifier = modifier
    ) {
        Icon(
            imageVector = Icons.Default.AttachFile,
            contentDescription = "Attach file",
            tint = if (enabled) {
                MaterialTheme.colorScheme.primary
            } else {
                MaterialTheme.colorScheme.onSurface.copy(alpha = 0.38f)
            }
        )
    }
}

/**
 * Button for attaching images with preview.
 */
@Composable
fun ImageAttachmentButton(
    onImageSelected: (Uri) -> Unit,
    enabled: Boolean = true,
    modifier: Modifier = Modifier
) {
    val imagePickerLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        uri?.let { onImageSelected(it) }
    }
    
    IconButton(
        onClick = { imagePickerLauncher.launch("image/*") },
        enabled = enabled,
        modifier = modifier
    ) {
        Icon(
            imageVector = ImageIcon,  // Would need to import this
            contentDescription = "Attach image",
            tint = if (enabled) {
                MaterialTheme.colorScheme.primary
            } else {
                MaterialTheme.colorScheme.onSurface.copy(alpha = 0.38f)
            }
        )
    }
}

/**
 * Row of attachment options.
 */
@Composable
fun AttachmentOptionsRow(
    onAttachFile: (Uri) -> Unit,
    onAttachImage: (Uri) -> Unit,
    enabled: Boolean = true,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 8.dp, vertical = 4.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        FileAttachmentButton(
            onFileSelected = onAttachFile,
            enabled = enabled
        )
        
        // Image picker would go here
        // ImageAttachmentButton(onImageSelected = onAttachImage, enabled = enabled)
    }
}

// Placeholder - would need actual icon import
private val ImageIcon: androidx.compose.ui.graphics.vector.ImageVector
    get() = Icons.Default.AttachFile  // Fallback
