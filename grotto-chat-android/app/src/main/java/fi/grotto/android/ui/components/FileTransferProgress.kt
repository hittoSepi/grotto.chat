package fi.grotto.android.ui.components

import androidx.compose.animation.core.*
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import fi.grotto.android.domain.model.FileTransfer
import fi.grotto.android.domain.model.FileType
import fi.grotto.android.domain.model.TransferDirection
import fi.grotto.android.domain.model.TransferStatus
import java.text.DecimalFormat

/**
 * Displays a file transfer with progress indicator.
 */
@Composable
fun FileTransferItem(
    transfer: FileTransfer,
    onCancel: () -> Unit,
    onRetry: () -> Unit,
    onClick: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    val fileType = FileType.fromFilename(transfer.filename)
    
    Card(
        onClick = onClick,
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 4.dp),
        colors = CardDefaults.cardColors(
            containerColor = when (transfer.status) {
                TransferStatus.FAILED -> MaterialTheme.colorScheme.errorContainer
                TransferStatus.COMPLETED -> MaterialTheme.colorScheme.surfaceVariant
                else -> MaterialTheme.colorScheme.surface
            }
        )
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // File icon
            FileIcon(fileType = fileType, modifier = Modifier.size(40.dp))
            
            Spacer(modifier = Modifier.width(12.dp))
            
            // File info
            Column(
                modifier = Modifier.weight(1f)
            ) {
                Text(
                    text = transfer.filename,
                    style = MaterialTheme.typography.bodyMedium,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                
                Text(
                    text = formatFileSize(transfer.fileSize),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                
                // Progress indicator
                if (transfer.isActive) {
                    LinearProgressIndicator(
                        progress = { transfer.progress },
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(top = 8.dp),
                    )
                    
                    Text(
                        text = "${transfer.progressPercent()}% • ${formatTransferStatus(transfer)}",
                        style = MaterialTheme.typography.labelSmall,
                        modifier = Modifier.padding(top = 4.dp)
                    )
                } else if (transfer.status == TransferStatus.COMPLETED) {
                    Text(
                        text = "Completed",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.primary
                    )
                } else if (transfer.status == TransferStatus.FAILED) {
                    Text(
                        text = transfer.errorMessage ?: "Failed",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.error
                    )
                }
            }
            
            Spacer(modifier = Modifier.width(8.dp))
            
            // Action button
            when (transfer.status) {
                TransferStatus.UPLOADING, TransferStatus.DOWNLOADING -> {
                    IconButton(onClick = onCancel) {
                        Icon(
                            imageVector = Icons.Default.Close,
                            contentDescription = "Cancel",
                            tint = MaterialTheme.colorScheme.error
                        )
                    }
                }
                TransferStatus.FAILED -> {
                    IconButton(onClick = onRetry) {
                        Icon(
                            imageVector = Icons.Default.Refresh,
                            contentDescription = "Retry",
                            tint = MaterialTheme.colorScheme.primary
                        )
                    }
                }
                TransferStatus.COMPLETED -> {
                    Icon(
                        imageVector = Icons.Default.CheckCircle,
                        contentDescription = "Completed",
                        tint = MaterialTheme.colorScheme.primary,
                        modifier = Modifier.size(24.dp)
                    )
                }
                else -> {
                    // Pending - show indeterminate indicator
                    CircularProgressIndicator(
                        modifier = Modifier.size(24.dp),
                        strokeWidth = 2.dp
                    )
                }
            }
        }
    }
}

/**
 * Compact file transfer indicator for inline display in chat.
 */
@Composable
fun InlineFileTransfer(
    transfer: FileTransfer,
    modifier: Modifier = Modifier
) {
    val fileType = FileType.fromFilename(transfer.filename)
    
    Surface(
        modifier = modifier,
        shape = MaterialTheme.shapes.small,
        color = MaterialTheme.colorScheme.surfaceVariant
    ) {
        Row(
            modifier = Modifier.padding(8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            FileIcon(fileType = fileType, modifier = Modifier.size(32.dp))
            
            Spacer(modifier = Modifier.width(8.dp))
            
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = transfer.filename,
                    style = MaterialTheme.typography.bodySmall,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                
                if (transfer.isActive) {
                    LinearProgressIndicator(
                        progress = { transfer.progress },
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(top = 4.dp),
                        strokeCap = androidx.compose.ui.graphics.StrokeCap.Round
                    )
                }
            }
            
            if (transfer.status == TransferStatus.COMPLETED) {
                Icon(
                    imageVector = Icons.Default.Check,
                    contentDescription = null,
                    modifier = Modifier.size(16.dp),
                    tint = MaterialTheme.colorScheme.primary
                )
            }
        }
    }
}

/**
 * File icon based on file type.
 */
@Composable
fun FileIcon(
    fileType: FileType,
    modifier: Modifier = Modifier
) {
    val icon = when (fileType) {
        FileType.IMAGE -> ImageIcon
        FileType.VIDEO -> PlayCircleIcon
        FileType.AUDIO -> MusicNoteIcon
        FileType.DOCUMENT -> DescriptionIcon
        FileType.SPREADSHEET -> TableChartIcon
        FileType.PRESENTATION -> SlideshowIcon
        FileType.ARCHIVE -> FolderZipIcon
        FileType.APK -> AndroidIcon  // May need custom
        FileType.CODE -> CodeIcon
        FileType.UNKNOWN -> InsertDriveFileIcon
    }
    
    Surface(
        modifier = modifier,
        shape = MaterialTheme.shapes.small,
        color = MaterialTheme.colorScheme.primaryContainer
    ) {
        Box(contentAlignment = Alignment.Center) {
            Icon(
                imageVector = icon,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.onPrimaryContainer,
                modifier = Modifier.size(24.dp)
            )
        }
    }
}

/**
 * List of active file transfers.
 */
@Composable
fun ActiveTransfersPanel(
    transfers: List<FileTransfer>,
    onCancelTransfer: (String) -> Unit,
    onRetryTransfer: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    if (transfers.isEmpty()) return
    
    Column(modifier = modifier) {
        Text(
            text = "Transfers (${transfers.size})",
            style = MaterialTheme.typography.labelMedium,
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp)
        )
        
        transfers.forEach { transfer ->
            FileTransferItem(
                transfer = transfer,
                onCancel = { onCancelTransfer(transfer.fileId) },
                onRetry = { onRetryTransfer(transfer.fileId) }
            )
        }
    }
}

// Helper functions

private fun formatFileSize(bytes: Long): String {
    val formatter = DecimalFormat("0.0")
    return when {
        bytes >= 1024 * 1024 * 1024 -> "${formatter.format(bytes / (1024.0 * 1024.0 * 1024.0))} GB"
        bytes >= 1024 * 1024 -> "${formatter.format(bytes / (1024.0 * 1024.0))} MB"
        bytes >= 1024 -> "${formatter.format(bytes / 1024.0)} KB"
        else -> "$bytes B"
    }
}

private fun formatTransferStatus(transfer: FileTransfer): String {
    return when (transfer.direction) {
        TransferDirection.UPLOAD -> "Uploading"
        TransferDirection.DOWNLOAD -> "Downloading"
    }
}

// Icon placeholders - these would need proper imports or custom implementations
private val ImageIcon: androidx.compose.ui.graphics.vector.ImageVector get() = InsertDriveFileIcon
// Fallback icons for missing material icons (using existing ones)
private val PlayCircleIcon: androidx.compose.ui.graphics.vector.ImageVector get() = Icons.Default.PlayArrow
private val MusicNoteIcon: androidx.compose.ui.graphics.vector.ImageVector get() = Icons.Default.Notifications
private val DescriptionIcon: androidx.compose.ui.graphics.vector.ImageVector get() = Icons.Default.Menu
private val TableChartIcon: androidx.compose.ui.graphics.vector.ImageVector get() = Icons.Default.Menu
private val SlideshowIcon: androidx.compose.ui.graphics.vector.ImageVector get() = Icons.Default.Menu
private val FolderZipIcon: androidx.compose.ui.graphics.vector.ImageVector get() = Icons.Default.Menu
private val AndroidIcon: androidx.compose.ui.graphics.vector.ImageVector get() = Icons.Default.Menu
private val CodeIcon: androidx.compose.ui.graphics.vector.ImageVector get() = Icons.Default.Menu
private val InsertDriveFileIcon: androidx.compose.ui.graphics.vector.ImageVector get() = Icons.Default.Menu
