package fi.grotto.android.data.local.entity

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.Index

/**
 * Channel membership entity - tracks users in channels with their roles.
 * Primary key is composite: (channel_id, user_id)
 */
@Entity(
    tableName = "channel_members",
    primaryKeys = ["channel_id", "user_id"],
    indices = [
        Index("channel_id"),
        Index("user_id"),
    ]
)
data class ChannelMemberEntity(
    @ColumnInfo(name = "channel_id") val channelId: String,
    @ColumnInfo(name = "user_id") val userId: String,
    @ColumnInfo(name = "nickname") val nickname: String,
    @ColumnInfo(name = "role") val role: String = "regular",  // op, voice, regular
    @ColumnInfo(name = "joined_at") val joinedAt: Long = System.currentTimeMillis(),
)

enum class ChannelRole {
    OP,      // @ - channel operator
    VOICE,   // + - voiced user
    REGULAR, // normal user
}

fun String.toChannelRole(): ChannelRole = when (this.lowercase()) {
    "op", "operator", "@" -> ChannelRole.OP
    "voice", "voiced", "+" -> ChannelRole.VOICE
    else -> ChannelRole.REGULAR
}

fun ChannelRole.toDisplayString(): String = when (this) {
    ChannelRole.OP -> "@"
    ChannelRole.VOICE -> "+"
    ChannelRole.REGULAR -> ""
}
