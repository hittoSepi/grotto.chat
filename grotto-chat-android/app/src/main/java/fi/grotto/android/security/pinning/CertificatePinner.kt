package fi.grotto.android.security.pinning

import okhttp3.Interceptor
import okhttp3.Response
import timber.log.Timber
import java.security.cert.X509Certificate
import javax.net.ssl.SSLPeerUnverifiedException

/**
 * OkHttp interceptor that validates certificate pins.
 * 
 * This interceptor checks that the server's certificate matches
 * one of the configured pins for the hostname.
 * 
 * Usage:
 * ```kotlin
 * val pinner = CertificatePinner(pinningConfig)
 * val client = OkHttpClient.Builder()
 *     .addInterceptor(pinner)
 *     .build()
 * ```
 */
class CertificatePinner(
    private val config: PinningConfig,
    private val onPinFailure: ((hostname: String, peerPins: List<String>) -> Unit)? = null,
) : Interceptor {

    override fun intercept(chain: Interceptor.Chain): Response {
        val request = chain.request()
        val hostname = request.url.host
        
        // Proceed with the request
        val response = chain.proceed(request)
        
        // Check certificate pins
        val connection = chain.connection()
        if (connection != null && connection.handshake() != null) {
            val handshake = connection.handshake()!!
            val certificates = handshake.peerCertificates
            
            if (certificates.isNotEmpty()) {
                val certificate = certificates[0] as X509Certificate
                val computedPin = CertificatePin.computePin(certificate)
                
                // Find pins for this hostname
                val matchingPins = config.findPinsForHostname(hostname)
                
                if (matchingPins.isNotEmpty()) {
                    // Validate against configured pins
                    val pinMatches = matchingPins.any { it.pin == computedPin }
                    
                    if (!pinMatches) {
                        val peerPins = certificates.map { 
                            CertificatePin.computePin(it as X509Certificate)
                        }
                        
                        Timber.w("Certificate pin mismatch for $hostname")
                        Timber.w("Expected one of: ${matchingPins.map { it.pin }}")
                        Timber.w("Got: $peerPins")
                        
                        onPinFailure?.invoke(hostname, peerPins)
                        
                        if (config.enforce && !config.reportOnly) {
                            throw SSLPeerUnverifiedException(
                                "Certificate pinning failure for $hostname. " +
                                "Expected pin matching ${matchingPins.first().pattern} " +
                                "but got $computedPin"
                            )
                        }
                    } else {
                        Timber.d("Certificate pin validated for $hostname")
                    }
                }
            }
        }
        
        return response
    }
}

/**
 * Event listener that validates certificate pins at the TLS handshake level.
 * This catches pinning failures earlier than the interceptor.
 */
class CertificatePinEventListener(
    private val config: PinningConfig,
    private val onPinValidated: ((hostname: String, pin: String) -> Unit)? = null,
    private val onPinFailure: ((hostname: String, peerPins: List<String>) -> Unit)? = null,
) : okhttp3.EventListener() {
    
    override fun secureConnectStart(call: okhttp3.Call) {
        super.secureConnectStart(call)
        Timber.v("Starting secure connection to ${call.request().url.host}")
    }
    
    override fun secureConnectEnd(call: okhttp3.Call, handshake: okhttp3.Handshake?) {
        super.secureConnectEnd(call, handshake)
        
        val hostname = call.request().url.host
        
        if (handshake != null) {
            val certificates = handshake.peerCertificates
            if (certificates.isNotEmpty()) {
                val certificate = certificates[0] as X509Certificate
                val computedPin = CertificatePin.computePin(certificate)
                
                val matchingPins = config.findPinsForHostname(hostname)
                
                if (matchingPins.isNotEmpty()) {
                    val pinMatches = matchingPins.any { it.pin == computedPin }
                    
                    if (pinMatches) {
                        Timber.d("Certificate pin validated for $hostname: $computedPin")
                        onPinValidated?.invoke(hostname, computedPin)
                    } else {
                        val peerPins = certificates.map { 
                            CertificatePin.computePin(it as X509Certificate)
                        }
                        
                        Timber.w("Certificate pin failure for $hostname")
                        onPinFailure?.invoke(hostname, peerPins)
                        
                        if (config.enforce && !config.reportOnly) {
                            throw SSLPeerUnverifiedException(
                                "Certificate pinning failure for $hostname"
                            )
                        }
                    }
                }
            }
        }
    }
}
