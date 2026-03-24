package fi.grotto.android.security.pinning

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import dagger.hilt.android.qualifiers.ApplicationContext
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import timber.log.Timber
import java.security.cert.X509Certificate
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Repository for managing certificate pins.
 * 
 * Stores pins in DataStore and provides methods to add, remove, and validate pins.
 * Supports first-use pinning (TOFU - Trust On First Use) for self-signed certificates.
 */
@Singleton
class PinRepository @Inject constructor(
    @ApplicationContext private val context: Context,
) {
    private val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "cert_pins")
    
    /**
     * Get the current pinning configuration.
     */
    val pinningConfig: Flow<PinningConfig> = context.dataStore.data.map { prefs ->
        val pinsJson = prefs[CERTIFICATE_PINS] ?: "[]"
        val enforce = prefs[ENFORCE_PINS] ?: true
        
        try {
            val pins = parsePinsJson(pinsJson)
            PinningConfig(pins, enforce)
        } catch (e: Exception) {
            Timber.e(e, "Failed to parse certificate pins")
            PinningConfig.empty()
        }
    }
    
    /**
     * Check if pinning is enabled.
     */
    val isPinningEnabled: Flow<Boolean> = context.dataStore.data.map { prefs ->
        prefs[ENFORCE_PINS] ?: true
    }
    
    /**
     * Add a certificate pin.
     * 
     * @param hostname The hostname pattern (e.g., "*.example.com")
     * @param pin The base64-encoded SHA-256 pin
     * @param isBackupPin true if this is a backup pin
     */
    suspend fun addPin(hostname: String, pin: String, isBackupPin: Boolean = false) {
        context.dataStore.edit { prefs ->
            val currentPins = parsePinsJson(prefs[CERTIFICATE_PINS] ?: "[]").toMutableList()
            
            // Remove existing pin for same hostname+pin combo
            currentPins.removeAll { it.pattern == hostname && it.pin == pin }
            
            // Add new pin
            currentPins.add(CertificatePin(hostname, pin, isBackupPin))
            
            prefs[CERTIFICATE_PINS] = pinsToJson(currentPins)
            Timber.i("Added certificate pin for $hostname")
        }
    }
    
    /**
     * Add a pin from a certificate (computes the pin automatically).
     */
    suspend fun addPinFromCertificate(hostname: String, certificate: X509Certificate, isBackupPin: Boolean = false) {
        val pin = CertificatePin.computePin(certificate)
        addPin(hostname, pin, isBackupPin)
    }
    
    /**
     * Remove a certificate pin.
     */
    suspend fun removePin(hostname: String, pin: String) {
        context.dataStore.edit { prefs ->
            val currentPins = parsePinsJson(prefs[CERTIFICATE_PINS] ?: "[]").toMutableList()
            currentPins.removeAll { it.pattern == hostname && it.pin == pin }
            prefs[CERTIFICATE_PINS] = pinsToJson(currentPins)
            Timber.i("Removed certificate pin for $hostname")
        }
    }
    
    /**
     * Remove all pins for a hostname.
     */
    suspend fun removeAllPinsForHostname(hostname: String) {
        context.dataStore.edit { prefs ->
            val currentPins = parsePinsJson(prefs[CERTIFICATE_PINS] ?: "[]").toMutableList()
            currentPins.removeAll { it.pattern == hostname }
            prefs[CERTIFICATE_PINS] = pinsToJson(currentPins)
            Timber.i("Removed all certificate pins for $hostname")
        }
    }
    
    /**
     * Clear all certificate pins.
     */
    suspend fun clearAllPins() {
        context.dataStore.edit { prefs ->
            prefs[CERTIFICATE_PINS] = "[]"
            Timber.i("Cleared all certificate pins")
        }
    }
    
    /**
     * Enable or disable certificate pinning.
     */
    suspend fun setPinningEnabled(enabled: Boolean) {
        context.dataStore.edit { prefs ->
            prefs[ENFORCE_PINS] = enabled
            Timber.i("Certificate pinning ${if (enabled) "enabled" else "disabled"}")
        }
    }
    
    /**
     * Trust a certificate on first use (TOFU).
     * 
     * If no pins exist for this hostname, adds the certificate's pin.
     * 
     * @param hostname The server hostname
     * @param certificate The server's certificate
     * @return true if the certificate was trusted (either matched existing pin or was added)
     */
    suspend fun trustOnFirstUse(hostname: String, certificate: X509Certificate): Boolean {
        val config = pinningConfig.map { config ->
            val matchingPins = config.findPinsForHostname(hostname)
            
            if (matchingPins.isEmpty()) {
                // No existing pins - add this certificate (TOFU)
                addPinFromCertificate(hostname, certificate)
                true
            } else {
                // Check if pin matches
                val computedPin = CertificatePin.computePin(certificate)
                matchingPins.any { it.pin == computedPin }
            }
        }
        
        return config.map { it }.toString().toBoolean()
    }
    
    /**
     * Get all pins for display/management.
     */
    fun getAllPins(): Flow<List<CertificatePin>> = context.dataStore.data.map { prefs ->
        parsePinsJson(prefs[CERTIFICATE_PINS] ?: "[]")
    }
    
    // JSON serialization for storing pins
    private fun parsePinsJson(json: String): List<CertificatePin> {
        if (json == "[]" || json.isBlank()) return emptyList()
        
        val pins = mutableListOf<CertificatePin>()
        
        // Simple JSON parsing for pin array
        // Format: [{"pattern":"host","pin":"abc","backup":false}]
        val regex = """\{"pattern":"([^"]+)","pin":"([^"]+)","backup":(true|false)\}""".toRegex()
        regex.findAll(json).forEach { match ->
            val pattern = match.groupValues[1]
            val pin = match.groupValues[2]
            val isBackup = match.groupValues[3].toBoolean()
            pins.add(CertificatePin(pattern, pin, isBackup))
        }
        
        return pins
    }
    
    private fun pinsToJson(pins: List<CertificatePin>): String {
        if (pins.isEmpty()) return "[]"
        
        val sb = StringBuilder("[")
        pins.forEachIndexed { index, pin ->
            if (index > 0) sb.append(",")
            sb.append("""{"pattern":"${pin.pattern}","pin":"${pin.pin}","backup":${pin.isBackupPin}}""")
        }
        sb.append("]")
        return sb.toString()
    }
    
    companion object {
        private val CERTIFICATE_PINS = stringPreferencesKey("certificate_pins")
        private val ENFORCE_PINS = androidx.datastore.preferences.core.booleanPreferencesKey("enforce_certificate_pins")
    }
}
