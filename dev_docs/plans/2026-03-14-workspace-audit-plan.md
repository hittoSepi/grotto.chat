# Workspace Audit and Delivery Plan

> Status: Phase 1 delivered on 2026-03-14
>
> Update 2026-04-07: Desktop voice is now functioning in the current workspace snapshot. The active readiness focus moved to server antivirus/checksum hardening and desktop file-transfer integrity. See `2026-04-07-desktop-server-readiness.md`.
>
> Scope: audit the current Grotto workspace, define a realistic implementation order, and ship the highest-value slice immediately instead of leaving the repo at planning-only state.

## Audit Summary

### `grotto-client`
- Windows configure/build works and the app already has login UI, voice UI, settings, search, mouse support, and a collapsible user list.
- The biggest readiness gaps were in polish rather than architecture:
  - `ircord://` CLI quick-connect was parsed in `main.cpp`, but the login form did not actually honor that prefill path.
  - FTXUI input only accepted the first byte/codepoint from `event.character()`, which broke multi-character paste and non-ASCII input.
  - User-list width/collapsed state existed in config structs, but the values were not fully loaded/saved/persisted end to end.

### `grotto-plugin`
- Library configures/builds on Windows and the existing test suite passes.
- The subsystem is still a foundation rather than a host-integrated feature:
  - `src/js_timers.cpp` is a stub.
  - `src/js_api.cpp` returns an empty object for `grotto.getConfig()`.
  - Plugin loading is not yet wired into the actual desktop/server runtime path in this workspace snapshot.

### `grotto-android`
- Large amount of design/foundation work exists, but the production path is still incomplete.
- The biggest unfinished areas are transport/service plumbing, JNI voice engine completion, file transfer, FCM server sync, and several chat UX actions.

### `grotto-infra` / `grotto-webclient`
- Directory + landing deploy path exists and recent memory notes show active progress.
- The main cleanup need is consolidation:
  - legacy `grotto-webclient` content still exists in parallel locations
  - documentation/deploy surface should be reduced to the supported path

## Priority Order

1. Desktop client readiness
2. Plugin system completeness
3. Infra/web cleanup
4. Android transport + voice production path
5. Documentation consistency and translation

## Phase 1: Desktop Client Readiness

### Goal
Make the desktop client match its current documentation and be reliable for daily use without changing the overall architecture.

### Delivered in this session
- Fix `ircord://` quick-connect so login prefill really reaches the UI.
- Fix input handling so pasted text and UTF-8 input are inserted correctly.
- Add clipboard paste shortcut handling (`Ctrl+V`) in the TUI input line.
- Persist user-list width/collapsed state through config load/save/export/import.
- Add a focused unit test target for `InputLine`.

### Files touched
- `grotto-client/src/app.cpp`
- `grotto-client/src/ui/login_screen.hpp`
- `grotto-client/src/ui/login_screen.cpp`
- `grotto-client/src/ui/input_line.hpp`
- `grotto-client/src/ui/input_line.cpp`
- `grotto-client/src/ui/mouse_support.hpp`
- `grotto-client/src/ui/mouse_support.cpp`
- `grotto-client/src/ui/ui_manager.hpp`
- `grotto-client/src/ui/ui_manager.cpp`
- `grotto-client/src/config.cpp`
- `grotto-client/CMakeLists.txt`
- `grotto-client/tests/test_input_line.cpp`
- `grotto-client/README.md`

### Verification
- `cmake -S grotto-client -B build/grotto-client-check`
- `cmake --build build/grotto-client-check --config Debug --target grotto-client test-input-line`
- `.\build\grotto-client-check\Debug\test-input-line.exe`

## Phase 2: Plugin System Completeness

### Goal
Move the plugin system from "buildable foundation" to "usable host feature".

### Next tasks
- Implement `setTimeout`, `setInterval`, `clearTimeout`, and `clearInterval` in `grotto-plugin/src/js_timers.cpp`.
- Implement real `grotto.getConfig()` object mapping in `grotto-plugin/src/js_api.cpp`.
- Expose a minimal shared timer/config abstraction through `PluginInstance`.
- Wire `PluginManager` into one real host path first:
  - recommended first target: desktop client local plugins
  - second target: server-side metadata plugins
- Add tests for timer execution, config visibility, and unload/reload behavior.

### Done when
- A sample plugin can register a timer, read config, and run inside a real host binary without manual test scaffolding.

## Phase 3: Infra and Web Cleanup

### Goal
Reduce duplicate/legacy surface area so deployment docs match the supported path.

### Next tasks
- Remove or clearly archive legacy `grotto-webclient` paths that are no longer used by deploy flow.
- Keep one canonical deploy README for `directory + landing + nginx`.
- Add one smoke-check script or doc section for:
  - directory API health
  - landing config generation
  - public server list flow

### Done when
- A new operator can follow one documented path without guessing which webclient/landing copy is authoritative.

## Phase 4: Android Production Path

### Goal
Focus on the minimum set that turns the Android client from foundation/demo state into a real interoperable client.

### Next tasks
- Finish connection/service lifecycle in `GrottoService` and socket/frame plumbing.
- Complete JNI/native voice engine path beyond current stubs.
- Finish FCM token registration/unregistration against the Grotto server protocol.
- Finish file transfer repository message handling.
- Resolve highest-impact UX gaps:
  - search action wiring
  - channel join flow
  - message retry/delete behavior

### Done when
- Android can connect, sync, exchange encrypted messages, receive push wakeups, and join voice with the same server as desktop.

## Phase 5: Documentation Consistency

### Goal
Make docs reflect the current product surface and reduce stale TODO drift.

### Next tasks
- Translate the Finnish architecture docs listed in root `TODO.md`.
- Refresh per-project TODO files so already-shipped features are not still presented as pending.
- Keep `docs/plans/` phase-based and date-stamped, then record concrete outcomes in daily memory.

## Note on FTXUI E2E Testing

- There is no direct Playwright-style browser automation equivalent for FTXUI itself.
- The practical equivalent is a pseudo-terminal harness:
  - launch the binary inside a PTY
  - send key sequences and pasted text
  - snapshot/assert terminal output frames
- For this repo, the recommended test pyramid is:
  - unit tests for pure input/state logic
  - PTY-driven integration tests for command flow and screen output
  - manual smoke tests for terminal-specific clipboard/mouse behavior
