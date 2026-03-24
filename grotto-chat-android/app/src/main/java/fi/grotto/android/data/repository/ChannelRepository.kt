package fi.grotto.android.data.repository

import fi.grotto.android.data.local.dao.ChannelDao
import fi.grotto.android.data.local.entity.ChannelEntity
import kotlinx.coroutines.flow.Flow
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class ChannelRepository @Inject constructor(
    private val channelDao: ChannelDao,
) {
    fun getAllChannels(): Flow<List<ChannelEntity>> = channelDao.getAllChannels()

    suspend fun getChannelById(channelId: String): ChannelEntity? = channelDao.getChannelById(channelId)

    suspend fun insert(channel: ChannelEntity) = channelDao.insert(channel)

    suspend fun updateLastRead(channelId: String, ts: Long) =
        channelDao.updateLastRead(channelId, ts)

    suspend fun updateTopic(channelId: String, topic: String?) =
        channelDao.updateTopic(channelId, topic)
}
