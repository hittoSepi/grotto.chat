package fi.grotto.android.domain.model

data class Channel(
    val channelId: String,
    val displayName: String,
    val type: ChannelType = ChannelType.TEXT,
    val unreadCount: Int = 0,
    val hasMention: Boolean = false,
    val members: List<String> = emptyList(),
)

enum class ChannelType { TEXT, VOICE, DIRECT }
