package fi.grotto.android.domain.model

data class Message(
    val id: Long = 0,
    val channelId: String,
    val senderId: String,
    val content: String,
    val timestamp: Long,
    val type: MessageType = MessageType.CHAT,
    val sendStatus: SendStatus = SendStatus.SENT,
    val linkPreview: LinkPreview? = null,
)

enum class MessageType { CHAT, SYSTEM, ACTION }

enum class SendStatus { SENDING, SENT, FAILED }

data class LinkPreview(
    val url: String,
    val title: String?,
    val description: String?,
    val imageUrl: String? = null,
)
