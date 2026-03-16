package fi.grotto.android.data.repository

import fi.grotto.android.data.local.dao.PeerIdentityDao
import fi.grotto.android.data.local.entity.PeerIdentityEntity
import kotlinx.coroutines.flow.Flow
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class KeyRepository @Inject constructor(
    private val peerIdentityDao: PeerIdentityDao,
) {
    suspend fun getPeerIdentity(userId: String): PeerIdentityEntity? =
        peerIdentityDao.getByUserId(userId)

    fun getAllPeers(): Flow<List<PeerIdentityEntity>> = peerIdentityDao.getAll()

    fun getOnlineUsers(): Flow<List<PeerIdentityEntity>> = peerIdentityDao.getOnlineUsers()

    suspend fun insertPeer(identity: PeerIdentityEntity) = peerIdentityDao.insert(identity)

    suspend fun markVerified(userId: String) = peerIdentityDao.updateTrust(userId, "verified")

    suspend fun updatePresence(userId: String, status: String) =
        peerIdentityDao.updatePresence(userId, status.lowercase())

    suspend fun getPresenceStatus(userId: String): String? =
        peerIdentityDao.getPresenceStatus(userId)
}
