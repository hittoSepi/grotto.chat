package fi.grotto.android.data.repository

import fi.grotto.android.data.local.dao.MessageDao
import fi.grotto.android.data.local.entity.MessageEntity
import kotlinx.coroutines.flow.Flow
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class MessageRepository @Inject constructor(
    private val messageDao: MessageDao,
) {
    fun getMessages(channelId: String): Flow<List<MessageEntity>> =
        messageDao.getMessagesForChannel(channelId)

    suspend fun insert(message: MessageEntity): Long =
        messageDao.insert(message)

    suspend fun updateSendStatus(id: Long, status: String) =
        messageDao.updateSendStatus(id, status)

    suspend fun getMessageById(id: Long): MessageEntity? =
        messageDao.getMessageById(id)

    suspend fun deleteById(id: Long) =
        messageDao.deleteById(id)

    suspend fun countUnread(channelId: String, afterTimestamp: Long): Int =
        messageDao.countUnread(channelId, afterTimestamp)

    suspend fun searchMessages(channelId: String, query: String): List<MessageEntity> =
        messageDao.searchMessages(channelId, query)
}
