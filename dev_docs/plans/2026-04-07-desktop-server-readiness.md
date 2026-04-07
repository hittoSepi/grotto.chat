# Desktop + Server Readiness

Status snapshot recorded on 2026-04-07.

## Summary

- Desktop voice is already operational in the current workspace snapshot.
- The next readiness slice is server hardening plus file-transfer integrity, not a new desktop voice implementation pass.
- Named desktop audio-device selection now resolves configured devices in the runtime backend and falls back to system defaults with an explicit warning if a requested device is missing.

## Delivered In This Slice

- Server-side ClamAV integration now uses the existing TCP / Unix-socket config surface instead of a stub.
- Antivirus is fail-closed when it is configured but unavailable or returns a scan error.
- Upload chunks can carry optional SHA-256 digests and the server now validates them when present.
- `MT_FILE_COMPLETE` now carries the final file checksum on upload and download paths.
- Desktop file transfer now emits chunk checksums and verifies the final checksum when the transfer completes.
- Desktop audio device selection now binds named input/output devices when they exist and degrades cleanly to defaults when they do not.
- A repeatable manual smoke flow is documented in `dev_docs/plans/2026-04-07-desktop-server-smoke-flow.md`.

## Remaining Follow-ups

- Expand server-side file-transfer tests further if a lighter-weight session harness is added later.
- Keep Android, plugin host integration, and infra cleanup on their separate tracks.
