# Error Scenario QA

This checklist is the repeatable manual QA path for client reconnect and shutdown work.

## Purpose

Use this when validating:

- server-down and reconnect behavior
- queued outbound messages during reconnect
- DM repair requests across reconnect
- graceful shutdown while voice is active
- graceful shutdown while file transfers are active

## Prepare

Windows:

```powershell
cmake --build build --config Release --target qa-error-scenarios
```

Linux:

```bash
cmake --build build --target qa-error-scenarios
```

This prepares an isolated QA config under `build/qa-error-scenarios/` and prints the exact launch commands for the built client.

## Core Scenarios

### 1. Server Down / Reconnect Queue

1. Start the server.
2. Start the client with the QA config prepared by the script.
3. Join any channel or DM.
4. Stop the server process.
5. Confirm the client stays alive and shows reconnecting state.
6. While reconnecting, send one chat message.
7. Confirm the status bar shows queued outbound work.
8. Start the server again.
9. Confirm the client reconnects, authenticates, and flushes the queued message.

Expected result:

- no crash
- no frozen UI
- queued message is delivered after reconnect/auth

### 2. DM Session Repair Across Reconnect

1. Create a DM situation that triggers session repair if available in your current test setup.
2. Stop the server before the repair completes.
3. Confirm the client keeps running and reconnects.
4. Start the server again.
5. Confirm the repair request is not dropped immediately just because reconnect happened.

Expected result:

- repair request is retried after reconnect
- client does not silently discard the pending DM repair flow

### 3. Quit During Voice Session

1. Start a voice room or direct call.
2. While voice is active, exit using `/quit`.
3. Repeat once with the window close action.
4. Repeat once via Settings -> Logout.

Expected result:

- client exits promptly
- no hung process remains
- next launch starts cleanly

### 4. Quit During File Transfer

1. Start an upload or download.
2. Exit using `/quit`.
3. Launch the client again.

Expected result:

- client exits promptly
- no stuck background worker remains
- next launch starts cleanly without broken transfer state causing instability

## Notes

- The PowerShell and shell helpers intentionally disable link previews in the QA config to keep runs deterministic.
- Keep `auto_reconnect = true` and `reconnect_delay_sec = 1` in the QA config for faster reconnect loops.
- If you want a clean sandbox for repeated runs, delete the generated `build/qa-error-scenarios/` directory and regenerate it with the target above.
