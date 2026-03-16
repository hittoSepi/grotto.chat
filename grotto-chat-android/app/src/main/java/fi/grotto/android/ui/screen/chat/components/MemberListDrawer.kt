package fi.grotto.android.ui.screen.chat.components

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.Chat
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.PersonOff
import androidx.compose.material.icons.filled.Shield
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import fi.grotto.android.data.local.entity.ChannelMemberEntity
import fi.grotto.android.data.local.entity.ChannelRole
import fi.grotto.android.data.local.entity.toChannelRole
import fi.grotto.android.data.local.entity.toDisplayString
import fi.grotto.android.domain.model.PresenceStatus
import fi.grotto.android.ui.theme.GrottoSpacing

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MemberListDrawer(
    channelId: String,
    onDismiss: () -> Unit,
    onUserClick: (ChannelMemberEntity) -> Unit,
    viewModel: MemberListViewModel = hiltViewModel(),
) {
    val members by viewModel.getMembers(channelId).collectAsState(initial = emptyList())
    val onlineStatuses by viewModel.onlineStatuses.collectAsState()

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = rememberModalBottomSheetState(),
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(GrottoSpacing.md)
        ) {
            Text(
                text = "Channel Members",
                style = MaterialTheme.typography.titleLarge,
                modifier = Modifier.padding(bottom = GrottoSpacing.sm)
            )
            Text(
                text = "${members.size} members",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(bottom = GrottoSpacing.md)
            )

            HorizontalDivider()

            LazyColumn(
                modifier = Modifier.fillMaxWidth()
            ) {
                items(
                    items = members.sortedWith(compareBy(
                        { it.role.toChannelRole().ordinal },
                        { it.nickname.lowercase() }
                    )),
                    key = { "${it.channelId}-${it.userId}" }
                ) { member ->
                    MemberItem(
                        member = member,
                        isOnline = onlineStatuses[member.userId] == PresenceStatus.ONLINE,
                        onClick = { onUserClick(member) }
                    )
                }
            }
        }
    }
}

@Composable
private fun MemberItem(
    member: ChannelMemberEntity,
    isOnline: Boolean,
    onClick: () -> Unit,
) {
    val role = member.role.toChannelRole()
    val roleColor = when (role) {
        ChannelRole.OP -> MaterialTheme.colorScheme.primary
        ChannelRole.VOICE -> MaterialTheme.colorScheme.secondary
        ChannelRole.REGULAR -> MaterialTheme.colorScheme.onSurfaceVariant
    }

    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick),
        color = MaterialTheme.colorScheme.surface
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = GrottoSpacing.sm, horizontal = GrottoSpacing.md),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Role icon
            when (role) {
                ChannelRole.OP -> Icon(
                    imageVector = Icons.Default.Shield,
                    contentDescription = "Operator",
                    tint = roleColor,
                    modifier = Modifier.size(20.dp)
                )
                ChannelRole.VOICE -> Icon(
                    imageVector = Icons.AutoMirrored.Filled.Chat,
                    contentDescription = "Voiced",
                    tint = roleColor,
                    modifier = Modifier.size(20.dp)
                )
                ChannelRole.REGULAR -> Icon(
                    imageVector = Icons.Default.Person,
                    contentDescription = "Member",
                    tint = roleColor,
                    modifier = Modifier.size(20.dp)
                )
            }

            Spacer(modifier = Modifier.width(GrottoSpacing.sm))

            // Role prefix (@, +)
            if (role != ChannelRole.REGULAR) {
                Text(
                    text = role.toDisplayString(),
                    color = roleColor,
                    fontWeight = FontWeight.Bold,
                    style = MaterialTheme.typography.bodyMedium
                )
            }

            // Nickname
            Text(
                text = member.nickname,
                style = MaterialTheme.typography.bodyLarge,
                modifier = Modifier.weight(1f)
            )

            // Online status indicator
            Icon(
                imageVector = if (isOnline) Icons.Default.Person else Icons.Default.PersonOff,
                contentDescription = if (isOnline) "Online" else "Offline",
                tint = if (isOnline) MaterialTheme.colorScheme.primary
                else MaterialTheme.colorScheme.outline,
                modifier = Modifier.size(16.dp)
            )
        }
    }
}
