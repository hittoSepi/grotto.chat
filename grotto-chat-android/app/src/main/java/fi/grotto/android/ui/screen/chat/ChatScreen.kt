package fi.grotto.android.ui.screen.chat

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material.icons.filled.People
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Circle
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.hilt.navigation.compose.hiltViewModel
import fi.grotto.android.data.local.entity.ChannelMemberEntity
import fi.grotto.android.ui.components.VoicePill
import fi.grotto.android.ui.screen.chat.components.MemberListDrawer
import fi.grotto.android.ui.screen.chat.components.MessageBubble
import fi.grotto.android.ui.screen.chat.components.MessageInput
import fi.grotto.android.ui.screen.chat.components.UserActionsMenu
import fi.grotto.android.ui.security.SecureScreenEffect
import fi.grotto.android.ui.theme.GrottoSpacing
import fi.grotto.android.ui.theme.GrottoTheme

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ChatScreen(
    channelId: String,
    onOpenDrawer: () -> Unit,
    onNavigateToSettings: () -> Unit,
    onNavigateToVoice: (String) -> Unit,
    viewModel: ChatViewModel = hiltViewModel(),
) {
    val state by viewModel.uiState.collectAsState()
    val listState = rememberLazyListState()
    
    // Member list state
    var showMemberList by remember { mutableStateOf(false) }
    var selectedMember by remember { mutableStateOf<ChannelMemberEntity?>(null) }
    
    // Apply screen security when screen capture is disabled
    SecureScreenEffect(enabled = !state.screenCaptureEnabled)

    // Scroll to bottom when keyboard opens or new messages arrive
    val currentMessages by rememberUpdatedState(state.messages)
    LaunchedEffect(currentMessages.size) {
        if (currentMessages.isNotEmpty()) {
            listState.animateScrollToItem(currentMessages.size - 1)
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Column {
                        Text(state.channelName, style = MaterialTheme.typography.titleMedium)
                        val topic = state.topic
                        if (!topic.isNullOrBlank()) {
                            Text(
                                text = topic,
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis,
                            )
                        }
                    }
                },
                navigationIcon = {
                    IconButton(onClick = onOpenDrawer) {
                        Icon(Icons.Default.Menu, contentDescription = "Channels")
                    }
                },
                actions = {
                    // Connection status indicator
                    val connectionColor = when {
                        state.isConnected -> GrottoTheme.semanticColors.statusOnline
                        else -> GrottoTheme.semanticColors.statusOffline
                    }
                    Icon(
                        Icons.Default.Circle,
                        contentDescription = if (state.isConnected) "Connected" else "Disconnected",
                        tint = connectionColor,
                        modifier = Modifier.padding(end = GrottoSpacing.sm)
                    )
                    if (state.isEncrypted) {
                        Icon(
                            Icons.Default.Lock,
                            contentDescription = "Encrypted",
                            tint = GrottoTheme.semanticColors.encryptionOk,
                        )
                    }
                    IconButton(onClick = { showMemberList = true }) {
                        Icon(Icons.Default.People, contentDescription = "Members")
                    }
                    IconButton(onClick = { viewModel.toggleSearch() }) {
                        Icon(
                            if (state.isSearchActive) Icons.Default.Close else Icons.Default.Search,
                            contentDescription = if (state.isSearchActive) "Close search" else "Search",
                        )
                    }
                    IconButton(onClick = onNavigateToSettings) {
                        Icon(Icons.Default.MoreVert, contentDescription = "More")
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surface,
                ),
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .imePadding() // Add padding for keyboard
                .navigationBarsPadding(), // Handle navigation bar
        ) {
            // Connection status banner
            if (!state.isConnected && !state.isConnecting) {
                Surface(
                    color = MaterialTheme.colorScheme.errorContainer,
                    modifier = Modifier.fillMaxWidth(),
                    onClick = { viewModel.reconnect() }
                ) {
                    Text(
                        text = "⚠ Disconnected - Tap to reconnect",
                        color = MaterialTheme.colorScheme.onErrorContainer,
                        style = MaterialTheme.typography.labelLarge,
                        modifier = Modifier.padding(GrottoSpacing.md)
                    )
                }
            } else if (state.isConnecting) {
                Surface(
                    color = MaterialTheme.colorScheme.secondaryContainer,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Text(
                        text = "⏳ Connecting...",
                        color = MaterialTheme.colorScheme.onSecondaryContainer,
                        style = MaterialTheme.typography.labelLarge,
                        modifier = Modifier.padding(GrottoSpacing.md)
                    )
                }
            }
            
            // Search bar
            if (state.isSearchActive) {
                OutlinedTextField(
                    value = state.searchQuery,
                    onValueChange = viewModel::onSearchQueryChanged,
                    placeholder = { Text("Search messages...") },
                    singleLine = true,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = GrottoSpacing.md, vertical = GrottoSpacing.xs),
                )
            }

            // Messages list - takes all available space
            val displayMessages = if (state.isSearchActive && state.searchQuery.length >= 2)
                state.searchResults
            else
                state.messages

            LazyColumn(
                state = listState,
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f), // Use weight to fill space
                reverseLayout = false,
            ) {
                items(displayMessages, key = { it.id }) { message ->
                    MessageBubble(
                        message = message,
                        onRetry = { id -> viewModel.retryMessage(id) },
                        onDelete = { id -> viewModel.deleteMessage(id) },
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(
                                horizontal = GrottoSpacing.messagePaddingHorizontal,
                                vertical = GrottoSpacing.messagePaddingVertical,
                            ),
                    )
                }
            }
            
            // Input area at bottom (moves with keyboard)
            Column(
                modifier = Modifier.fillMaxWidth()
            ) {
                if (state.voiceActive && state.voiceChannelName != null) {
                    VoicePill(
                        channelName = state.voiceChannelName!!,
                        participantCount = state.voiceParticipantCount,
                        onJoin = { onNavigateToVoice(channelId) },
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
                MessageInput(
                    text = state.inputText,
                    onTextChanged = viewModel::onInputChanged,
                    onSend = viewModel::sendMessage,
                    onAttachFile = viewModel::uploadFile,
                    enabled = state.isConnected,
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        }

        // Member list drawer
        if (showMemberList) {
            MemberListDrawer(
                channelId = channelId,
                onDismiss = { showMemberList = false },
                onUserClick = { member ->
                    selectedMember = member
                },
            )
        }

        // User actions menu
        selectedMember?.let { member ->
            UserActionsMenu(
                member = member,
                currentUserRole = state.currentUserRole,
                onDismiss = { selectedMember = null },
                onSendDM = { nickname ->
                    viewModel.sendCommand("query", nickname)
                },
                onWhois = { nickname ->
                    viewModel.sendCommand("whois", nickname)
                },
                onKick = { nickname ->
                    viewModel.sendCommand("kick", nickname)
                },
                onBan = { nickname ->
                    viewModel.sendCommand("ban", nickname)
                },
            )
        }
    }
}
