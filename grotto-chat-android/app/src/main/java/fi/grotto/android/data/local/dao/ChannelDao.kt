package fi.grotto.android.data.local.dao

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import fi.grotto.android.data.local.entity.ChannelEntity
import kotlinx.coroutines.flow.Flow

@Dao
interface ChannelDao {

    @Query("SELECT * FROM channels ORDER BY display_name ASC")
    fun getAllChannels(): Flow<List<ChannelEntity>>

    @Query("SELECT * FROM channels WHERE channel_id = :channelId")
    suspend fun getChannelById(channelId: String): ChannelEntity?

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insert(channel: ChannelEntity)

    @Query("DELETE FROM channels WHERE channel_id = :channelId")
    suspend fun delete(channelId: String)

    @Query("UPDATE channels SET last_read_ts = :ts WHERE channel_id = :channelId")
    suspend fun updateLastRead(channelId: String, ts: Long)

    @Query("UPDATE channels SET topic = :topic WHERE channel_id = :channelId")
    suspend fun updateTopic(channelId: String, topic: String?)
}
