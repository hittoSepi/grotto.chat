package fi.grotto.android.domain.model

import java.util.UUID

/**
 * Represents a file transfer operation (upload or download).
 */
data class FileTransfer(
    val id: String = UUID.randomUUID().toString(),
    val fileId: String,
    val filename: String,
    val mimeType: String,
    val fileSize: Long,
    val direction: TransferDirection,
    val status: TransferStatus = TransferStatus.PENDING,
    val progress: Float = 0f,
    val bytesTransferred: Long = 0,
    val recipientId: String? = null,
    val channelId: String? = null,
    val localUri: String? = null,  // Local file path/URI
    val errorMessage: String? = null,
    val timestamp: Long = System.currentTimeMillis()
) {
    val isComplete: Boolean
        get() = status == TransferStatus.COMPLETED
    
    val isFailed: Boolean
        get() = status == TransferStatus.FAILED
    
    val isActive: Boolean
        get() = status == TransferStatus.UPLOADING || status == TransferStatus.DOWNLOADING
    
    fun progressPercent(): Int = (progress * 100).toInt()
}

enum class TransferDirection {
    UPLOAD,     // Client -> Server
    DOWNLOAD    // Server -> Client
}

enum class TransferStatus {
    PENDING,      // Waiting to start
    UPLOADING,    // Currently uploading
    DOWNLOADING,  // Currently downloading
    COMPLETED,    // Transfer finished successfully
    FAILED,       // Transfer failed
    CANCELLED     // User cancelled
}

/**
 * File metadata received from server.
 */
data class FileMetadata(
    val fileId: String,
    val filename: String,
    val fileSize: Long,
    val mimeType: String,
    val senderId: String,
    val recipientId: String?,
    val channelId: String?,
    val uploadedAt: Long,
    val expiresAt: Long?
)

/**
 * File chunk for chunked transfers.
 */
data class FileChunk(
    val fileId: String,
    val chunkIndex: Int,
    val data: ByteArray,
    val isLast: Boolean
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false
        
        other as FileChunk
        
        if (fileId != other.fileId) return false
        if (chunkIndex != other.chunkIndex) return false
        if (!data.contentEquals(other.data)) return false
        if (isLast != other.isLast) return false
        
        return true
    }
    
    override fun hashCode(): Int {
        var result = fileId.hashCode()
        result = 31 * result + chunkIndex
        result = 31 * result + data.contentHashCode()
        result = 31 * result + isLast.hashCode()
        return result
    }
}

/**
 * Supported file types with icons and colors.
 */
enum class FileType(
    val extensions: List<String>,
    val icon: String,  // Material icon name
    val isImage: Boolean = false,
    val isVideo: Boolean = false,
    val isAudio: Boolean = false,
    val isDocument: Boolean = false
) {
    IMAGE(
        listOf("jpg", "jpeg", "png", "gif", "webp", "bmp", "svg"),
        "image",
        isImage = true
    ),
    VIDEO(
        listOf("mp4", "webm", "mkv", "avi", "mov"),
        "videocam",
        isVideo = true
    ),
    AUDIO(
        listOf("mp3", "ogg", "wav", "flac", "aac", "m4a"),
        "audiotrack",
        isAudio = true
    ),
    DOCUMENT(
        listOf("pdf", "doc", "docx", "txt", "rtf"),
        "description",
        isDocument = true
    ),
    SPREADSHEET(
        listOf("xls", "xlsx", "csv"),
        "table_chart",
        isDocument = true
    ),
    PRESENTATION(
        listOf("ppt", "pptx"),
        "slideshow",
        isDocument = true
    ),
    ARCHIVE(
        listOf("zip", "rar", "7z", "tar", "gz"),
        "folder_zip"
    ),
    APK(
        listOf("apk"),
        "android"
    ),
    CODE(
        listOf("js", "ts", "kt", "java", "cpp", "h", "py", "rb", "go", "rs"),
        "code"
    ),
    UNKNOWN(
        emptyList(),
        "insert_drive_file"
    );
    
    companion object {
        fun fromFilename(filename: String): FileType {
            val extension = filename.substringAfterLast(".", "").lowercase()
            return entries.find { it.extensions.contains(extension) } ?: UNKNOWN
        }
        
        fun fromMimeType(mimeType: String): FileType {
            return when {
                mimeType.startsWith("image/") -> IMAGE
                mimeType.startsWith("video/") -> VIDEO
                mimeType.startsWith("audio/") -> AUDIO
                mimeType == "application/pdf" -> DOCUMENT
                mimeType.contains("spreadsheet") || mimeType.contains("excel") -> SPREADSHEET
                mimeType.contains("presentation") || mimeType.contains("powerpoint") -> PRESENTATION
                mimeType.contains("zip") || mimeType.contains("compressed") -> ARCHIVE
                mimeType == "application/vnd.android.package-archive" -> APK
                mimeType.startsWith("text/") -> CODE
                else -> UNKNOWN
            }
        }
    }
}

/**
 * Maximum file sizes for different types.
 */
object FileTransferLimits {
    const val MAX_FILE_SIZE = 100 * 1024 * 1024L  // 100 MB
    const val CHUNK_SIZE = 64 * 1024              // 64 KB chunks
    const val MAX_CONCURRENT_UPLOADS = 3
    const val MAX_CONCURRENT_DOWNLOADS = 5
    
    // Image compression limits
    const val MAX_IMAGE_DIMENSION = 2048
    const val COMPRESSED_IMAGE_QUALITY = 85
}
