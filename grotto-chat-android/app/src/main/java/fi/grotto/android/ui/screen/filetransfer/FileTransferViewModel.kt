package fi.grotto.android.ui.screen.filetransfer

import android.net.Uri
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import fi.grotto.android.data.repository.FileRepository
import fi.grotto.android.domain.model.FileTransfer
import fi.grotto.android.domain.model.TransferStatus
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import timber.log.Timber
import javax.inject.Inject

/**
 * ViewModel for file transfer operations.
 */
@HiltViewModel
class FileTransferViewModel @Inject constructor(
    private val fileRepository: FileRepository
) : ViewModel() {
    
    // All transfers
    private val _transfers = MutableStateFlow<List<FileTransfer>>(emptyList())
    val transfers: StateFlow<List<FileTransfer>> = _transfers.asStateFlow()
    
    // Active (uploading/downloading) transfers
    val activeTransfers: StateFlow<List<FileTransfer>> = _transfers
        .map { list -> list.filter { it.isActive } }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(), emptyList())
    
    // Completed transfers
    val completedTransfers: StateFlow<List<FileTransfer>> = _transfers
        .map { list -> list.filter { it.isComplete } }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(), emptyList())
    
    // Failed transfers
    val failedTransfers: StateFlow<List<FileTransfer>> = _transfers
        .map { list -> list.filter { it.isFailed } }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(), emptyList())
    
    // UI events
    private val _events = MutableSharedFlow<FileTransferEvent>()
    val events: SharedFlow<FileTransferEvent> = _events.asSharedFlow()
    
    init {
        // Collect updates from repository
        viewModelScope.launch {
            fileRepository.transferUpdates.collect { transfer ->
                updateTransferList(transfer)
                
                // Emit events for important state changes
                when (transfer.status) {
                    TransferStatus.COMPLETED -> {
                        _events.emit(FileTransferEvent.Success(transfer.fileId))
                    }
                    TransferStatus.FAILED -> {
                        _events.emit(FileTransferEvent.Error(
                            transfer.fileId,
                            transfer.errorMessage ?: "Transfer failed"
                        ))
                    }
                    else -> { }
                }
            }
        }
    }
    
    /**
     * Upload a file.
     */
    fun uploadFile(uri: Uri, recipientId: String? = null, channelId: String? = null) {
        viewModelScope.launch {
            _events.emit(FileTransferEvent.Starting)
            
            fileRepository.uploadFile(uri, recipientId, channelId)
                .onSuccess { transfer ->
                    addTransfer(transfer)
                    _events.emit(FileTransferEvent.UploadStarted(transfer.fileId))
                }
                .onFailure { error ->
                    Timber.e(error, "Failed to start upload")
                    _events.emit(FileTransferEvent.Error("", error.message ?: "Failed to start upload"))
                }
        }
    }
    
    /**
     * Download a file.
     */
    fun downloadFile(fileId: String) {
        viewModelScope.launch {
            _events.emit(FileTransferEvent.Starting)
            
            fileRepository.downloadFile(fileId)
                .onSuccess { transfer ->
                    addTransfer(transfer)
                    _events.emit(FileTransferEvent.DownloadStarted(fileId))
                }
                .onFailure { error ->
                    Timber.e(error, "Failed to start download")
                    _events.emit(FileTransferEvent.Error(fileId, error.message ?: "Failed to start download"))
                }
        }
    }
    
    /**
     * Cancel a transfer.
     */
    fun cancelTransfer(fileId: String) {
        fileRepository.cancelTransfer(fileId)
    }
    
    /**
     * Retry a failed transfer.
     */
    fun retryTransfer(fileId: String) {
        val transfer = fileRepository.getTransfer(fileId) ?: return
        
        when (transfer.direction) {
            fi.grotto.android.domain.model.TransferDirection.UPLOAD -> {
                transfer.localUri?.let { uri ->
                    uploadFile(
                        Uri.parse(uri),
                        transfer.recipientId,
                        transfer.channelId
                    )
                }
            }
            fi.grotto.android.domain.model.TransferDirection.DOWNLOAD -> {
                downloadFile(fileId)
            }
        }
    }
    
    /**
     * Clear completed and failed transfers from the list.
     */
    fun clearCompletedTransfers() {
        fileRepository.clearCompletedTransfers()
        _transfers.value = _transfers.value.filter { 
            !it.isComplete && !it.isFailed && it.status != TransferStatus.CANCELLED
        }
    }
    
    /**
     * Get a file for sharing/opening.
     */
    fun getFileForSharing(fileId: String): java.io.File? {
        return fileRepository.getLocalFile(fileId)
    }
    
    private fun addTransfer(transfer: FileTransfer) {
        _transfers.value = _transfers.value + transfer
    }
    
    private fun updateTransferList(updated: FileTransfer) {
        _transfers.value = _transfers.value.map {
            if (it.fileId == updated.fileId) updated else it
        }
    }
    
    override fun onCleared() {
        super.onCleared()
        fileRepository.cleanup()
    }
}

/**
 * UI events for file transfers.
 */
sealed class FileTransferEvent {
    object Starting : FileTransferEvent()
    data class UploadStarted(val fileId: String) : FileTransferEvent()
    data class DownloadStarted(val fileId: String) : FileTransferEvent()
    data class Success(val fileId: String) : FileTransferEvent()
    data class Error(val fileId: String, val message: String) : FileTransferEvent()
}
