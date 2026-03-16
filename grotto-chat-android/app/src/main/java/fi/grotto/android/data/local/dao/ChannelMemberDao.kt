package fi.grotto.android.data.local.dao

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import fi.grotto.android.data.local.entity.ChannelMemberEntity
import kotlinx.coroutines.flow.Flow

@Dao
interface ChannelMemberDao {

    @Query("SELECT * FROM channel_members WHERE channel_id = :channelId ORDER BY role DESC, nickname ASC")
    fun getMembersForChannel(channelId: String): Flow<List<ChannelMemberEntity>>

    @Query("SELECT * FROM channel_members WHERE channel_id = :channelId AND user_id = :userId LIMIT 1")
    suspend fun getMember(channelId: String, userId: String): ChannelMemberEntity?

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insert(member: ChannelMemberEntity)

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertAll(members: List<ChannelMemberEntity>)

    @Query("DELETE FROM channel_members WHERE channel_id = :channelId AND user_id = :userId")
    suspend fun deleteMember(channelId: String, userId: String)

    @Query("DELETE FROM channel_members WHERE channel_id = :channelId")
    suspend fun deleteAllForChannel(channelId: String)

    @Query("UPDATE channel_members SET nickname = :newNickname WHERE user_id = :userId")
    suspend fun updateNickname(userId: String, newNickname: String)

    @Query("UPDATE channel_members SET role = :role WHERE channel_id = :channelId AND user_id = :userId")
    suspend fun updateRole(channelId: String, userId: String, role: String)

    @Query("SELECT COUNT(*) FROM channel_members WHERE channel_id = :channelId")
    suspend fun getMemberCount(channelId: String): Int

    @Query("SELECT * FROM channel_members WHERE user_id = :userId")
    suspend fun getChannelsForUser(userId: String): List<ChannelMemberEntity>
}
