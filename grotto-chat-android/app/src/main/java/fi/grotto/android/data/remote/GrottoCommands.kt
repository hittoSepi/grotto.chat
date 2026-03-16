package fi.grotto.android.data.remote

import grotto.Grotto.Envelope
import grotto.Grotto.IrcCommand
import grotto.Grotto.MessageType
import grotto.Grotto.CommandResponse
import grotto.Grotto.NickChange
import grotto.Grotto.UserInfo
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeoutOrNull
import timber.log.Timber
import kotlin.coroutines.resume

/**
 * IRC command sender and parser.
 * Handles sending IRC-style commands to the server.
 */
class GrottoCommands(
    private val socket: GrottoSocket,
    private val frameCodec: FrameCodec,
) {
    /**
     * Parse and send an IRC command from user input.
     * Returns true if the command was handled as an IRC command.
     */
    suspend fun sendCommand(input: String): Boolean {
        if (!input.startsWith("/")) {
            return false
        }

        val parts = input.substring(1).split(" ")
        if (parts.isEmpty()) {
            return false
        }

        val command = parts[0].lowercase()
        val args = parts.drop(1)

        return when (command) {
            "join" -> sendIrcCommand("join", args)
            "part", "leave" -> sendIrcCommand("part", args)
            "nick" -> sendIrcCommand("nick", args)
            "whois" -> sendIrcCommand("whois", args)
            "me", "action" -> sendIrcCommand("me", args)
            "topic" -> sendIrcCommand("topic", args)
            "kick" -> sendIrcCommand("kick", args)
            "ban" -> sendIrcCommand("ban", args)
            "invite" -> sendIrcCommand("invite", args)
            "set" -> sendIrcCommand("set", args)
            "mode" -> sendIrcCommand("mode", args)
            "msg", "query" -> sendIrcCommand("msg", args)
            "quit" -> sendIrcCommand("quit", args)
            else -> {
                Timber.w("Unknown command: $command")
                false
            }
        }
    }

    private suspend fun sendIrcCommand(command: String, args: List<String>): Boolean {
        val cmd = IrcCommand.newBuilder()
            .setCommand(command)
            .addAllArgs(args)
            .build()

        val envelope = Envelope.newBuilder()
            .setType(MessageType.MT_COMMAND)
            .setPayload(cmd.toByteString())
            .build()

        return socket.send(envelope.toByteArray())
    }

    /**
     * Parse command response from server.
     */
    fun parseCommandResponse(payload: ByteArray): CommandResponse? {
        return try {
            CommandResponse.parseFrom(payload)
        } catch (e: Exception) {
            Timber.e(e, "Failed to parse command response")
            null
        }
    }

    /**
     * Parse nick change notification.
     */
    fun parseNickChange(payload: ByteArray): NickChange? {
        return try {
            NickChange.parseFrom(payload)
        } catch (e: Exception) {
            Timber.e(e, "Failed to parse nick change")
            null
        }
    }

    /**
     * Parse WHOIS response.
     */
    fun parseUserInfo(payload: ByteArray): UserInfo? {
        return try {
            UserInfo.parseFrom(payload)
        } catch (e: Exception) {
            Timber.e(e, "Failed to parse user info")
            null
        }
    }

    companion object {
        /**
         * Check if input is a command (starts with /).
         */
        fun isCommand(input: String): Boolean = input.startsWith("/")

        /**
         * Get help text for a command.
         */
        fun getHelp(command: String): String = when (command.lowercase()) {
            "join" -> "/join <#channel> - Join a channel"
            "part", "leave" -> "/part <#channel> [message] - Leave a channel"
            "nick" -> "/nick <new_nick> - Change your nickname"
            "whois" -> "/whois <nick> - Show user information"
            "me" -> "/me <action> - Send an action message"
            "topic" -> "/topic <#channel> [new_topic] - View or change channel topic"
            "kick" -> "/kick <#channel> <nick> [reason] - Kick a user (op only)"
            "ban" -> "/ban <#channel> <nick> - Ban a user (op only)"
            "invite" -> "/invite <#channel> <nick> [message] - Invite a user (op only)"
            "set" -> "/set <#channel> <option> <value> - Change channel settings (op only)"
            "mode" -> "/mode <#channel> <+o|-o|+v|-v> <nick> - Change user mode (op only)"
            "msg" -> "/msg <nick> <message> - Send a private message"
            "quit" -> "/quit [message] - Disconnect from server"
            else -> "Unknown command: $command"
        }

        /**
         * Get list of available commands.
         */
        fun getAvailableCommands(): List<String> = listOf(
            "join", "part", "leave", "nick", "whois", "me", "action",
            "topic", "kick", "ban", "invite", "set", "mode", "msg", "quit"
        )
    }
}
