package fi.grotto.android.security.pinning

import java.security.MessageDigest
import java.security.cert.X509Certificate

/**
 * Represents a certificate pin for a specific hostname.
 * 
 * A pin is the base64-encoded SHA-256 hash of a certificate's public key (SPKI).
 * This follows the HTTP Public Key Pinning (HPKP) approach but at the app level.
 * 
 * @param pattern The hostname pattern (e.g., "*.example.com" or "grotto.example.com")
 * @param pin The base64-encoded SHA-256 hash of the public key
 * @param isBackupPin true if this is a backup pin (for certificate rotation)
 */
data class CertificatePin(
    val pattern: String,
    val pin: String,
    val isBackupPin: Boolean = false,
) {
    companion object {
        /**
         * Compute the SHA-256 pin of a certificate's public key.
         * 
         * @param certificate The X509 certificate
         * @return Base64-encoded SHA-256 hash of the SPKI
         */
        fun computePin(certificate: X509Certificate): String {
            val publicKey = certificate.publicKey
            val encoded = publicKey.encoded
            val digest = MessageDigest.getInstance("SHA-256").digest(encoded)
            return java.util.Base64.getEncoder().encodeToString(digest)
        }
        
        /**
         * Parse a pin from a string in the format "sha256/AAAA..." or just "AAAA..."
         */
        fun parsePin(pinString: String): String {
            return if (pinString.startsWith("sha256/")) {
                pinString.substring(7)
            } else {
                pinString
            }
        }
        
        /**
         * Check if a hostname matches a pattern.
         * Supports wildcards like *.example.com
         */
        fun matchesPattern(hostname: String, pattern: String): Boolean {
            val normalizedHostname = hostname.lowercase()
            val normalizedPattern = pattern.lowercase()
            
            return when {
                normalizedPattern == normalizedHostname -> true
                normalizedPattern.startsWith("*.") -> {
                    val suffix = normalizedPattern.substring(1)
                    normalizedHostname.endsWith(suffix) && 
                    normalizedHostname.indexOf('.') == normalizedHostname.lastIndexOf('.') - suffix.length + 1
                }
                else -> false
            }
        }
    }
    
    /**
     * Check if the given hostname matches this pin's pattern.
     */
    fun matches(hostname: String): Boolean = matchesPattern(hostname, pattern)
}

/**
 * Configuration for certificate pinning.
 * 
 * @param pins List of certificate pins
 * @param enforce true to enforce pinning (fail connection if pins don't match)
 * @param reportOnly true to only report violations (for testing)
 */
data class PinningConfig(
    val pins: List<CertificatePin>,
    val enforce: Boolean = true,
    val reportOnly: Boolean = false,
) {
    /**
     * Find matching pins for a hostname.
     */
    fun findPinsForHostname(hostname: String): List<CertificatePin> {
        return pins.filter { it.matches(hostname) }
    }
    
    /**
     * Check if a certificate's pin matches any of the configured pins for the hostname.
     * 
     * @param hostname The server hostname
     * @param certificate The server's certificate
     * @return true if the pin matches or no pins are configured for this hostname
     */
    fun validateCertificate(hostname: String, certificate: X509Certificate): Boolean {
        val hostnamePins = findPinsForHostname(hostname)
        
        // If no pins configured for this hostname, allow (or reject based on policy)
        if (hostnamePins.isEmpty()) {
            return true // Allow connections to hosts without pins
        }
        
        val computedPin = CertificatePin.computePin(certificate)
        return hostnamePins.any { it.pin == computedPin }
    }
    
    companion object {
        /**
         * Create an empty configuration (no pinning).
         */
        fun empty(): PinningConfig = PinningConfig(emptyList())
        
        /**
         * Create a configuration with a single pin.
         */
        fun single(hostname: String, pin: String): PinningConfig {
            return PinningConfig(listOf(CertificatePin(hostname, pin)))
        }
    }
}
