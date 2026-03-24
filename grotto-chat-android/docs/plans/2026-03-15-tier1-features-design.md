# Android Tier 1 Features — Design

## Goal
Add all missing Tier 1 features to the Android client: full command system, missing message type handlers, message retry, unread counts, and action message display.

## Architecture
Extend existing patterns — no new architecture layers needed. ChatViewModel.handleCommand() and GrottoConnectionManager.dispatch() already use clean `when` blocks that just need more branches.

## What Already Works
- `/join`, `/part`, `/nick`, `/me` commands
- MT_COMMAND_RESPONSE, MT_ERROR, MT_PING handlers
- MessageType.ACTION in domain model + DB mapping
- ChannelEntity.lastReadTs field + ChannelDao.updateLastRead()
- sendCommand() wraps everything in IrcCommand proto

## Changes Required

### 1. Proto sync — add MotdMessage
Android proto is missing `MotdMessage { repeated string lines = 1; }` and `MT_MOTD = 75` enum value.
Copy from server proto.

### 2. Command expansion (ChatViewModel.handleCommand)
Add `when` branches for: `/whois`, `/topic`, `/names`, `/kick`, `/ban`, `/invite`, `/password`, `/quit`, `/msg`
Each calls `connectionManager.sendCommand(cmd, *args)`.

### 3. Message type handlers (GrottoConnectionManager.dispatch)
- **MT_NICK_CHANGE** → parse NickChange, invoke callback
- **MT_MOTD** → parse MotdMessage, invoke callback
- **MT_USER_INFO** → parse UserInfo, invoke callback

### 4. Message retry (ChatViewModel.retryMessage)
Query message by ID from DB, re-send via connectionManager.sendChat().

### 5. Unread count (MessageDao + ChannelListViewModel)
Add `countUnread(channelId, afterTimestamp)` DAO query.
Replace hardcoded `unreadCount = 0` with actual count.

### 6. Action message display (MessageBubble)
Check `message.type == ACTION`, render as `* sender content` in italic.

## Files Modified
- `app/src/main/proto/grotto.proto` — add MotdMessage
- `ChatViewModel.kt` — handleCommand expansion, retryMessage impl
- `GrottoConnectionManager.kt` — dispatch + handler methods
- `MessageDao.kt` — add countUnread query, getMessageById query
- `ChannelListViewModel.kt` — unread count calculation
- `MessageBubble.kt` — action message rendering
