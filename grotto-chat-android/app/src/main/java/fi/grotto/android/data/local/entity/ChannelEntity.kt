package fi.grotto.android.data.local.entity

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity(tableName = "channels")
data class ChannelEntity(
    @PrimaryKey @ColumnInfo(name = "channel_id") val channelId: String,
    @ColumnInfo(name = "display_name") val displayName: String?,
    @ColumnInfo(name = "joined_at") val joinedAt: Long,
    @ColumnInfo(name = "last_read_ts") val lastReadTs: Long = 0,
    @ColumnInfo(name = "topic") val topic: String? = null,
)
