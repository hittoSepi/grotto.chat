package fi.grotto.android.domain.model

data class User(
    val userId: String,
    val nickname: String = userId,
    val status: PresenceStatus = PresenceStatus.OFFLINE,
    val identityFingerprint: String? = null,
    val trustStatus: TrustStatus = TrustStatus.UNVERIFIED,
)

enum class PresenceStatus { ONLINE, AWAY, OFFLINE }

enum class TrustStatus { VERIFIED, UNVERIFIED, WARNING }
