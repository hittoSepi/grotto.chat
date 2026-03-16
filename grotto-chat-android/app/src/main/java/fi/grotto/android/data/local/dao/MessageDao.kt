package fi.grotto.android.data.local.dao

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import fi.grotto.android.data.local.entity.MessageEntity
import kotlinx.coroutines.flow.Flow

@Dao
interface MessageDao {

    @Query("SELECT * FROM messages WHERE channel_id = :channelId ORDER BY timestamp ASC")
    fun getMessagesForChannel(channelId: String): Flow<List<MessageEntity>>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insert(message: MessageEntity): Long

    @Query("UPDATE messages SET send_status = :status WHERE id = :id")
    suspend fun updateSendStatus(id: Long, status: String)

    @Query("DELETE FROM messages WHERE channel_id = :channelId")
    suspend fun deleteByChannel(channelId: String)

    @Query("SELECT * FROM messages WHERE id = :id LIMIT 1")
    suspend fun getMessageById(id: Long): MessageEntity?

    @Query("DELETE FROM messages WHERE id = :id")
    suspend fun deleteById(id: Long)

    @Query("SELECT COUNT(*) FROM messages WHERE channel_id = :channelId AND timestamp > :afterTimestamp")
    suspend fun countUnread(channelId: String, afterTimestamp: Long): Int

    @Query("SELECT * FROM messages WHERE channel_id = :channelId AND content LIKE '%' || :query || '%' ORDER BY timestamp DESC LIMIT 50")
    suspend fun searchMessages(channelId: String, query: String): List<MessageEntity>
}
