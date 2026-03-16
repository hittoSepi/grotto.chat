package fi.grotto.android.data.remote

import javax.inject.Inject
import kotlin.math.min
import kotlin.math.pow

/**
 * Exponential backoff reconnect policy.
 */
class ReconnectPolicy @Inject constructor() {

    companion object {
        const val BASE_DELAY_MS = 1000L
        const val MAX_DELAY_MS = 30000L
        const val MAX_ATTEMPTS = 20
    }

    private var attempt = 0

    fun nextDelayMs(): Long {
        val delay = min(
            BASE_DELAY_MS * 2.0.pow(attempt).toLong(),
            MAX_DELAY_MS
        )
        attempt++
        return delay
    }

    fun reset() {
        attempt = 0
    }

    fun hasAttemptsLeft(): Boolean = attempt < MAX_ATTEMPTS
}
