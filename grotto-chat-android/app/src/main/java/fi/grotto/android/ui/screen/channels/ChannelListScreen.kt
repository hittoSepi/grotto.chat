package fi.grotto.android.ui.screen.channels

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.MicOff
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Tag
import androidx.compose.material.icons.filled.VolumeUp
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import fi.grotto.android.ui.components.StatusBadge
import fi.grotto.android.ui.components.UserAvatar
import fi.grotto.android.ui.theme.GrottoSpacing
import fi.grotto.android.ui.theme.GrottoTheme

@Composable
fun ChannelListScreen(
    onChannelSelected: (String) -> Unit,
    onNavigateToSettings: () -> Unit,
    viewModel: ChannelListViewModel = hiltViewModel(),
) {
    val state by viewModel.uiState.collectAsState()
    val semantic = GrottoTheme.semanticColors

    // Show join channel dialog when requested
    if (state.showJoinDialog) {
        JoinChannelDialog(
            availableChannels = state.availableChannels,
            dialogState = state.joinDialogState,
            onJoin = { channelName ->
                viewModel.joinChannel(channelName)
            },
            onDismiss = {
                viewModel.hideJoinDialog()
            },
        )
    }

    Scaffold(
        modifier = Modifier.fillMaxSize(),
        floatingActionButton = {
            FloatingActionButton(
                onClick = { viewModel.showJoinDialog() },
                containerColor = MaterialTheme.colorScheme.primary,
                contentColor = MaterialTheme.colorScheme.onPrimary,
            ) {
                Icon(
                    imageVector = Icons.Default.Add,
                    contentDescription = "Join channel",
                )
            }
        },
    ) { paddingValues ->
        Surface(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues),
        ) {
            Column(
                modifier = Modifier
                    .width(GrottoSpacing.drawerWidth)
                    .verticalScroll(rememberScrollState())
                    .padding(vertical = GrottoSpacing.lg),
            ) {
                // Current user
                Row(
                    modifier = Modifier.padding(horizontal = GrottoSpacing.channelItemPadding),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    StatusBadge(status = state.currentUser.status)
                    Spacer(Modifier.width(GrottoSpacing.sm))
                    Text(state.currentUser.nickname, style = MaterialTheme.typography.titleMedium)
                }

                Spacer(Modifier.height(GrottoSpacing.lg))

                // Text Channels
                SectionHeader("TEXT CHANNELS")
                state.textChannels.forEach { channel ->
                    ChannelItem(
                        icon = { Icon(Icons.Default.Tag, null, Modifier.size(GrottoSpacing.channelIconSize)) },
                        name = channel.displayName,
                        unreadCount = channel.unreadCount,
                        onClick = { onChannelSelected(channel.channelId) },
                    )
                }

                Spacer(Modifier.height(GrottoSpacing.lg))
                HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
                Spacer(Modifier.height(GrottoSpacing.sm))

                // Voice Channels
                SectionHeader("VOICE CHANNELS")
                state.voiceChannels.forEach { vc ->
                    ChannelItem(
                        icon = { Icon(Icons.Default.VolumeUp, null, Modifier.size(GrottoSpacing.channelIconSize)) },
                        name = vc.channel.displayName,
                        onClick = { onChannelSelected(vc.channel.channelId) },
                    )
                    vc.participants.forEach { p ->
                        Row(
                            modifier = Modifier
                                .padding(start = GrottoSpacing.xxl, end = GrottoSpacing.channelItemPadding)
                                .height(28.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Text("\u251C ", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                            Text(p.userId, style = MaterialTheme.typography.bodySmall)
                            Spacer(Modifier.width(GrottoSpacing.xs))
                            Icon(
                                if (p.isMuted) Icons.Default.MicOff else Icons.Default.Mic,
                                null,
                                modifier = Modifier.size(14.dp),
                                tint = if (p.isMuted) semantic.voiceMuted else semantic.voiceActive,
                            )
                        }
                    }
                    if (vc.participants.isEmpty()) {
                        Text(
                            "(empty)",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.padding(start = GrottoSpacing.xxl),
                        )
                    }
                }

                Spacer(Modifier.height(GrottoSpacing.lg))
                HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
                Spacer(Modifier.height(GrottoSpacing.sm))

                // Direct Messages
                SectionHeader("DIRECT MESSAGES")
                state.directMessages.forEach { user ->
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(GrottoSpacing.channelItemHeight)
                            .clickable { onChannelSelected(user.userId) }
                            .padding(horizontal = GrottoSpacing.channelItemPadding),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        StatusBadge(status = user.status)
                        Spacer(Modifier.width(GrottoSpacing.sm))
                        Text(user.nickname, style = MaterialTheme.typography.bodyMedium)
                    }
                }

                Spacer(Modifier.weight(1f))
                HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)

                TextButton(
                    onClick = onNavigateToSettings,
                    modifier = Modifier.padding(horizontal = GrottoSpacing.channelItemPadding),
                ) {
                    Icon(Icons.Default.Settings, null)
                    Spacer(Modifier.width(GrottoSpacing.sm))
                    Text("Settings")
                }
            }
        }
    }
}

@Composable
private fun SectionHeader(title: String) {
    Text(
        text = title,
        style = MaterialTheme.typography.labelMedium,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        modifier = Modifier.padding(
            horizontal = GrottoSpacing.channelItemPadding,
            vertical = GrottoSpacing.xs,
        ),
    )
}

@Composable
private fun ChannelItem(
    icon: @Composable () -> Unit,
    name: String,
    unreadCount: Int = 0,
    onClick: () -> Unit,
) {
    val semantic = GrottoTheme.semanticColors

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(GrottoSpacing.channelItemHeight)
            .clickable(onClick = onClick)
            .padding(horizontal = GrottoSpacing.channelItemPadding),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        icon()
        Spacer(Modifier.width(GrottoSpacing.sm))
        Text(name, style = MaterialTheme.typography.titleMedium, modifier = Modifier.weight(1f))
        if (unreadCount > 0) {
            Surface(
                color = semantic.unreadBadge,
                shape = MaterialTheme.shapes.extraSmall,
            ) {
                Text(
                    text = unreadCount.toString(),
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onError,
                    modifier = Modifier.padding(horizontal = GrottoSpacing.xs, vertical = GrottoSpacing.xxs),
                )
            }
        }
    }
}
