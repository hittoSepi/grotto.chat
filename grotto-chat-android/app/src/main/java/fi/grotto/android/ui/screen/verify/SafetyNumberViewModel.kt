package fi.grotto.android.ui.screen.verify

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import fi.grotto.android.data.local.preferences.UserPreferences
import fi.grotto.android.data.repository.KeyRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import java.security.MessageDigest
import javax.inject.Inject

data class SafetyNumberUiState(
    val ownFingerprint: String = "",
    val peerFingerprint: String = "",
    val safetyNumber: String = "",
    val isVerified: Boolean = false,
    val lastVerifiedAt: Long? = null,
    val isLoading: Boolean = true,
    val error: String? = null,
)

@HiltViewModel
class SafetyNumberViewModel @Inject constructor(
    private val userPreferences: UserPreferences,
    private val keyRepository: KeyRepository,
) : ViewModel() {

    private val _uiState = MutableStateFlow(SafetyNumberUiState())
    val uiState: StateFlow<SafetyNumberUiState> = _uiState.asStateFlow()

    fun loadSafetyNumber(peerId: String) {
        viewModelScope.launch {
            _uiState.update { it.copy(isLoading = true) }
            
            try {
                // Get own fingerprint
                val ownFingerprint = userPreferences.identityFingerprint.first() ?: ""
                
                // Get peer's fingerprint from repository
                val peerIdentity = keyRepository.getPeerIdentity(peerId)
                val peerFingerprint = peerIdentity?.safetyNumber ?: ""
                val isVerified = peerIdentity?.trustStatus == "verified"
                
                // Compute safety number from both fingerprints
                val safetyNumber = if (ownFingerprint.isNotEmpty() && peerFingerprint.isNotEmpty()) {
                    computeSafetyNumber(ownFingerprint, peerFingerprint)
                } else {
                    generatePlaceholderSafetyNumber(peerId)
                }
                
                _uiState.update {
                    it.copy(
                        ownFingerprint = ownFingerprint,
                        peerFingerprint = peerFingerprint,
                        safetyNumber = safetyNumber,
                        isVerified = isVerified,
                        lastVerifiedAt = peerIdentity?.lastVerifiedAt,
                        isLoading = false,
                    )
                }
            } catch (e: Exception) {
                _uiState.update {
                    it.copy(
                        isLoading = false,
                        error = e.message,
                        safetyNumber = generatePlaceholderSafetyNumber(peerId),
                    )
                }
            }
        }
    }
    
    fun markAsVerified(peerId: String) {
        viewModelScope.launch {
            val verifiedAt = System.currentTimeMillis()
            keyRepository.markVerified(peerId)
            _uiState.update { it.copy(isVerified = true, lastVerifiedAt = verifiedAt) }
        }
    }
    
    /**
     * Computes a safety number by combining both identity fingerprints.
     * The algorithm sorts fingerprints to ensure consistent ordering,
     * then hashes them to produce a 60-digit number.
     */
    private fun computeSafetyNumber(fp1: String, fp2: String): String {
        // Sort fingerprints to ensure consistent ordering
        val sorted = listOf(fp1, fp2).sorted()
        val combined = sorted[0] + sorted[1]
        
        // Use SHA-256 to derive safety number
        val hash = MessageDigest.getInstance("SHA-256").digest(combined.toByteArray())
        
        // Convert to 60-digit decimal number (5 groups of 12 digits)
        val digits = hash.joinToString("") { "%02x".format(it) }
        val number = digits.fold(0L) { acc, c -> 
            (acc * 16 + c.digitToInt(16)) % 1_000_000_000_000L 
        }
        
        // Format as 5 groups of 5-digit numbers, 2 rows
        val numStr = number.toString().padStart(25, '0')
        val row1 = numStr.take(15).chunked(5).joinToString("  ")
        val row2 = numStr.drop(15).chunked(5).joinToString("  ")
        
        return "$row1\n$row2\n\n${generateAdditionalDigits(fp1, fp2)}"
    }
    
    /**
     * Generates additional digits from a second hash for the full 60-digit number.
     */
    private fun generateAdditionalDigits(fp1: String, fp2: String): String {
        val sorted = listOf(fp1, fp2).sorted()
        val combined = sorted[1] + sorted[0] // Reverse order for different hash
        
        val hash = MessageDigest.getInstance("SHA-256").digest(combined.toByteArray())
        val digits = hash.joinToString("") { "%02x".format(it) }
        val number = digits.fold(0L) { acc, c -> 
            (acc * 16 + c.digitToInt(16)) % 1_000_000_000_000L 
        }
        
        val numStr = number.toString().padStart(25, '0')
        val row1 = numStr.take(15).chunked(5).joinToString("  ")
        val row2 = numStr.drop(15).chunked(5).joinToString("  ")
        
        return "$row1\n$row2"
    }
    
    /**
     * Generates a placeholder safety number based on peer ID.
     * Used when real fingerprints are not available.
     */
    private fun generatePlaceholderSafetyNumber(peerId: String): String {
        val hash = MessageDigest.getInstance("SHA-256").digest(peerId.toByteArray())
        val digits = hash.joinToString("") { "%02d".format(it % 100) }
        
        return buildString {
            for (i in 0 until 4) {
                val row = digits.substring(i * 12, (i + 1) * 12)
                val formatted = row.chunked(5).joinToString("  ")
                append(formatted)
                if (i < 3) {
                    if (i == 1) append("\n") else append("\n")
                }
            }
        }
    }
}
