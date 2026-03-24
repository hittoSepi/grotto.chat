package fi.grotto.android.ui.screen.chat.components

import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.Send
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.Shield
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.ListItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import fi.grotto.android.data.local.entity.ChannelMemberEntity
import fi.grotto.android.data.local.entity.ChannelRole
import fi.grotto.android.data.local.entity.toChannelRole
import fi.grotto.android.ui.theme.GrottoSpacing

@Composable
fun UserActionsMenu(
    member: ChannelMemberEntity,
    currentUserRole: ChannelRole,
    onDismiss: () -> Unit,
    onSendDM: (String) -> Unit,
    onWhois: (String) -> Unit,
    onKick: (String) -> Unit,
    onBan: (String) -> Unit,
) {
    var showKickConfirm by remember { mutableStateOf(false) }
    var showBanConfirm by remember { mutableStateOf(false) }

    val canModerate = currentUserRole == ChannelRole.OP && member.role.toChannelRole() != ChannelRole.OP

    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Text(
                text = member.nickname,
                fontWeight = FontWeight.Bold
            )
        },
        text = {
            Column(
                modifier = Modifier.fillMaxWidth()
            ) {
                // Send DM
                ListItem(
                    headlineContent = { Text("Send Message") },
                    leadingContent = {
                        Icon(
                            imageVector = Icons.AutoMirrored.Filled.Send,
                            contentDescription = null
                        )
                    },
                    modifier = Modifier.clickableNoRipple {
                        onSendDM(member.nickname)
                        onDismiss()
                    }
                )

                // Whois
                ListItem(
                    headlineContent = { Text("User Info") },
                    leadingContent = {
                        Icon(
                            imageVector = Icons.Default.Info,
                            contentDescription = null
                        )
                    },
                    modifier = Modifier.clickableNoRipple {
                        onWhois(member.nickname)
                        onDismiss()
                    }
                )

                // Moderation actions (only for ops targeting non-ops)
                if (canModerate) {
                    HorizontalDivider(
                        modifier = Modifier.padding(vertical = GrottoSpacing.sm)
                    )

                    ListItem(
                        headlineContent = { 
                            Text(
                                "Kick User",
                                color = MaterialTheme.colorScheme.error
                            )
                        },
                        leadingContent = {
                            Icon(
                                imageVector = Icons.Default.Person,
                                contentDescription = null,
                                tint = MaterialTheme.colorScheme.error
                            )
                        },
                        modifier = Modifier.clickableNoRipple {
                            showKickConfirm = true
                        }
                    )

                    ListItem(
                        headlineContent = { 
                            Text(
                                "Ban User",
                                color = MaterialTheme.colorScheme.error
                            )
                        },
                        leadingContent = {
                            Icon(
                                imageVector = Icons.Default.Shield,
                                contentDescription = null,
                                tint = MaterialTheme.colorScheme.error
                            )
                        },
                        modifier = Modifier.clickableNoRipple {
                            showBanConfirm = true
                        }
                    )
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel")
            }
        }
    )

    // Kick confirmation
    if (showKickConfirm) {
        AlertDialog(
            onDismissRequest = { showKickConfirm = false },
            title = { Text("Kick User") },
            text = { Text("Are you sure you want to kick ${member.nickname}?") },
            confirmButton = {
                TextButton(
                    onClick = {
                        onKick(member.nickname)
                        showKickConfirm = false
                        onDismiss()
                    }
                ) {
                    Text("Kick", color = MaterialTheme.colorScheme.error)
                }
            },
            dismissButton = {
                TextButton(onClick = { showKickConfirm = false }) {
                    Text("Cancel")
                }
            }
        )
    }

    // Ban confirmation
    if (showBanConfirm) {
        AlertDialog(
            onDismissRequest = { showBanConfirm = false },
            title = { Text("Ban User") },
            text = { Text("Are you sure you want to ban ${member.nickname}?") },
            confirmButton = {
                TextButton(
                    onClick = {
                        onBan(member.nickname)
                        showBanConfirm = false
                        onDismiss()
                    }
                ) {
                    Text("Ban", color = MaterialTheme.colorScheme.error)
                }
            },
            dismissButton = {
                TextButton(onClick = { showBanConfirm = false }) {
                    Text("Cancel")
                }
            }
        )
    }
}

// Helper extension to avoid ripple effect on clickable list items
private fun Modifier.clickableNoRipple(onClick: () -> Unit): Modifier = this.then(
    clickable(
        interactionSource = MutableInteractionSource(),
        indication = null,
        onClick = onClick
    )
)
