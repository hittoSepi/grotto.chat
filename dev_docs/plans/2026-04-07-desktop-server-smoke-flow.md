# Desktop + Server Smoke Flow

Repeat this flow after touching desktop voice, file transfer, server antivirus, or protocol-level file completion.

## Preconditions

- Build `grotto-server` and `grotto-client` from the current workspace.
- Have two desktop client configs pointing at the same server.
- Prepare one known-clean file and one test file that ClamAV should flag in the target environment.
- If you want to verify named audio-device binding, set `[voice].input_device` and `[voice].output_device` in one client config to real device names from the local machine.

## Flow

1. Start `grotto-server` with antivirus disabled and connect both desktop clients.
2. Send the known-clean file from client A to client B.
3. Confirm on client B that the transfer reaches `MT_FILE_COMPLETE` without a checksum error and the downloaded file opens correctly.
4. Corrupt the transfer path deliberately by forcing a checksum mismatch in a debug build or test harness and confirm the receiver rejects the transfer with a checksum failure.
5. Restart `grotto-server` with ClamAV configured and available.
6. Upload the same known-clean file again and confirm the transfer still succeeds.
7. Upload the known-detected file and confirm the server rejects it before delivery.
8. Start voice on one desktop client with named devices configured and confirm the runtime logs either the resolved device names or a fallback warning to system defaults.

## Expected Result

- Clean upload/download succeeds.
- Final file checksum is present on completion and verified by the receiving client.
- Corrupted transfers fail closed on the client.
- Infected uploads fail closed on the server.
- Voice still opens successfully after the file-transfer and antivirus changes, with explicit logging for named-device resolution or fallback.
