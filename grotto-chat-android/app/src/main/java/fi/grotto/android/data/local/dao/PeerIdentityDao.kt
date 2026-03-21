package fi.grotto.android.data.local.dao

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import fi.grotto.android.data.local.entity.PeerIdentityEntity
import kotlinx.coroutines.flow.Flow

@Dao
interface PeerIdentityDao {

    @Query("SELECT * FROM peer_identities WHERE user_id = :userId")
    suspend fun getByUserId(userId: String): PeerIdentityEntity?

    @Query("SELECT * FROM peer_identities")
    fun getAll(): Flow<List<PeerIdentityEntity>>

    @Query("SELECT * FROM peer_identities WHERE presence_status = 'online' OR presence_status = 'away'")
    fun getOnlineUsers(): Flow<List<PeerIdentityEntity>>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insert(identity: PeerIdentityEntity)

    @Query("UPDATE peer_identities SET trust_status = :status, last_verified_at = :timestamp WHERE user_id = :userId")
    suspend fun updateTrust(userId: String, status: String, timestamp: Long?)

    @Query("UPDATE peer_identities SET presence_status = :status, presence_updated_at = :timestamp WHERE user_id = :userId")
    suspend fun updatePresence(userId: String, status: String, timestamp: Long = System.currentTimeMillis())

    @Query("SELECT presence_status FROM peer_identities WHERE user_id = :userId")
    suspend fun getPresenceStatus(userId: String): String?
}
