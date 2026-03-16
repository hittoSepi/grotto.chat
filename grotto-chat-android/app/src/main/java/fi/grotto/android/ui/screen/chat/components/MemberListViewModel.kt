package fi.grotto.android.ui.screen.chat.components

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import fi.grotto.android.data.local.entity.ChannelMemberEntity
import fi.grotto.android.data.repository.ChannelMemberRepository
import fi.grotto.android.data.repository.KeyRepository
import fi.grotto.android.domain.model.PresenceStatus
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.launch
import timber.log.Timber
import javax.inject.Inject

@HiltViewModel
class MemberListViewModel @Inject constructor(
    private val channelMemberRepository: ChannelMemberRepository,
    private val keyRepository: KeyRepository,
) : ViewModel() {

    private val _onlineStatuses = MutableStateFlow<Map<String, PresenceStatus>>(emptyMap())
    val onlineStatuses: StateFlow<Map<String, PresenceStatus>> = _onlineStatuses.asStateFlow()

    init {
        // Observe online users and update status map
        keyRepository.getOnlineUsers()
            .onEach { users ->
                val statusMap = users.associate { user ->
                    val status = when (user.presenceStatus.lowercase()) {
                        "online" -> PresenceStatus.ONLINE
                        "away" -> PresenceStatus.AWAY
                        else -> PresenceStatus.OFFLINE
                    }
                    user.userId to status
                }
                _onlineStatuses.value = statusMap
            }
            .launchIn(viewModelScope)
    }

    fun getMembers(channelId: String): Flow<List<ChannelMemberEntity>> {
        return channelMemberRepository.getMembersForChannel(channelId)
    }

    fun syncMemberList(channelId: String, namesResponse: String) {
        viewModelScope.launch {
            try {
                channelMemberRepository.syncMemberList(channelId, namesResponse)
                Timber.d("Synced member list for $channelId")
            } catch (e: Exception) {
                Timber.e(e, "Failed to sync member list for $channelId")
            }
        }
    }

    fun addMember(channelId: String, userId: String, nickname: String, role: String = "regular") {
        viewModelScope.launch {
            try {
                channelMemberRepository.addMember(channelId, userId, nickname, role)
                Timber.d("Added member $nickname to $channelId")
            } catch (e: Exception) {
                Timber.e(e, "Failed to add member $nickname to $channelId")
            }
        }
    }

    fun removeMember(channelId: String, userId: String) {
        viewModelScope.launch {
            try {
                channelMemberRepository.removeMember(channelId, userId)
                Timber.d("Removed member $userId from $channelId")
            } catch (e: Exception) {
                Timber.e(e, "Failed to remove member $userId from $channelId")
            }
        }
    }

    fun updateNickname(userId: String, newNickname: String) {
        viewModelScope.launch {
            try {
                channelMemberRepository.updateNickname(userId, newNickname)
                Timber.d("Updated nickname for $userId to $newNickname")
            } catch (e: Exception) {
                Timber.e(e, "Failed to update nickname for $userId")
            }
        }
    }
}
