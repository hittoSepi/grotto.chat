package fi.grotto.android.data.repository

import fi.grotto.android.data.local.dao.ChannelMemberDao
import fi.grotto.android.data.local.entity.ChannelMemberEntity
import fi.grotto.android.data.local.entity.ChannelRole
import kotlinx.coroutines.flow.Flow
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class ChannelMemberRepository @Inject constructor(
    private val channelMemberDao: ChannelMemberDao,
) {
    fun getMembersForChannel(channelId: String): Flow<List<ChannelMemberEntity>> =
        channelMemberDao.getMembersForChannel(channelId)

    suspend fun getMember(channelId: String, userId: String): ChannelMemberEntity? =
        channelMemberDao.getMember(channelId, userId)

    suspend fun addMember(channelId: String, userId: String, nickname: String, role: String = "regular") {
        channelMemberDao.insert(
            ChannelMemberEntity(
                channelId = channelId,
                userId = userId,
                nickname = nickname,
                role = role.lowercase(),
            )
        )
    }

    suspend fun addMembers(members: List<ChannelMemberEntity>) {
        channelMemberDao.insertAll(members)
    }

    suspend fun removeMember(channelId: String, userId: String) {
        channelMemberDao.deleteMember(channelId, userId)
    }

    suspend fun clearChannelMembers(channelId: String) {
        channelMemberDao.deleteAllForChannel(channelId)
    }

    suspend fun updateNickname(userId: String, newNickname: String) {
        channelMemberDao.updateNickname(userId, newNickname)
    }

    suspend fun updateRole(channelId: String, userId: String, role: ChannelRole) {
        channelMemberDao.updateRole(channelId, userId, role.name.lowercase())
    }

    suspend fun getMemberCount(channelId: String): Int =
        channelMemberDao.getMemberCount(channelId)

    /**
     * Parse member list from server response and update database.
     * Expected format: "@op1 +voice1 regular1 regular2 @op2"
     */
    suspend fun syncMemberList(channelId: String, namesResponse: String) {
        // Clear existing members first
        channelMemberDao.deleteAllForChannel(channelId)

        val members = parseNamesList(channelId, namesResponse)
        if (members.isNotEmpty()) {
            channelMemberDao.insertAll(members)
        }
    }

    private fun parseNamesList(channelId: String, response: String): List<ChannelMemberEntity> {
        val members = mutableListOf<ChannelMemberEntity>()
        val tokens = response.split(" ", "\t", "\n").filter { it.isNotBlank() }

        for (token in tokens) {
            val trimmed = token.trim()
            if (trimmed.isEmpty()) continue

            val (role, nickname) = when {
                trimmed.startsWith("@") -> "op" to trimmed.substring(1)
                trimmed.startsWith("+") -> "voice" to trimmed.substring(1)
                else -> "regular" to trimmed
            }

            // Use nickname as userId if no separate ID provided
            members.add(
                ChannelMemberEntity(
                    channelId = channelId,
                    userId = nickname.lowercase(),  // Normalize userId
                    nickname = nickname,
                    role = role,
                )
            )
        }

        return members
    }
}
