# Grotto Android

Android client for Grotto — an end-to-end encrypted chat and voice application for friend groups. Built with Kotlin, Jetpack Compose, and C++ (NDK).

## Features

- 🔒 **End-to-end encryption** via Signal Protocol (X3DH + Double Ratchet)
- 👥 **Group chats** with Sender Keys for efficient multi-party encryption
- 🎨 **Dark/Light themes** — Tokyo Night dark theme + clean light theme
- 🔔 **Push notifications** — Firebase Cloud Messaging integration
- 🎙️ **Voice calls** — WebRTC-based voice rooms and private calls
- 🔐 **Ed25519 identity keys** with Argon2id-encrypted storage
- 🛡️ **Biometric authentication** — optional fingerprint unlock
- 🚫 **Screen capture protection** — FLAG_SECURE option
- 🔗 **Link previews** — automatic URL fetching
- 📎 **File transfers** — upload/download with progress

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  UI Layer (Jetpack Compose)                                  │
│  - Material3 design system                                   │
│  - Theme support (System/Light/Dark)                         │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────┴──────────────────────────────────┐
│  ViewModel Layer (Kotlin Coroutines)                         │
│  - ThemeViewModel, ChatViewModel, SettingsViewModel          │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────┴──────────────────────────────────┐
│  Repository Layer                                            │
│  - CryptoRepository (Signal Protocol via JNI)                │
│  - MessageRepository, ChannelRepository                      │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────┴──────────────────────────────────┐
│  Native Layer (C++ NDK)                                      │
│  - libsignal-protocol-c (X3DH, Double Ratchet)               │
│  - libsodium (Argon2id, XChaCha20-Poly1305)                  │
└─────────────────────────────────────────────────────────────┘
```

## Technology Stack

| Component | Technology |
|-----------|------------|
| Language | Kotlin / C++20 |
| UI | Jetpack Compose (Material3) |
| Architecture | MVVM + Repository pattern |
| DI | Hilt |
| Database | Room (SQLite) with SQLCipher encryption |
| Preferences | DataStore |
| Networking | OkHttp + raw TLS socket |
| Crypto (native) | libsignal-protocol-c + libsodium |
| Serialization | Protobuf Lite |
| Push | Firebase Cloud Messaging |
| Build | Gradle + CMake |

## Build Instructions

### Prerequisites

- Android Studio Hedgehog (2023.1.1) or newer
- Android SDK 34
- NDK 27.0.12077973 or newer
- CMake 3.22.1 or newer

### Build

```bash
cd grotto-android

# Debug build
./gradlew assembleDebug

# Release build
./gradlew assembleRelease

# Run tests
./gradlew test

# Install on connected device
./gradlew installDebug
```

The first build will:
1. Download dependencies (Gradle, Android SDK)
2. Fetch and build `libsignal-protocol-c` via CMake FetchContent
3. Compile native `grotto-native.so` libraries for all ABIs
4. Generate Java/Kotlin classes from protobuf schemas
5. Build the Android app

## Documentation Screenshots

The repo includes a Maestro workspace for website/documentation screenshots in [`.maestro/`](./.maestro).

Run from `grotto-android`:

```bash
./gradlew installDebug
maestro test .maestro
```

The flows launch debug-only screenshot scenes through app launch arguments, so setup/chat/settings/voice captures are deterministic and do not need a live server session. Output is written under `.maestro/artifacts/screenshots/android/`.

### Screenshot Scenes

| Scene | Description |
|-------|-------------|
| `setup` | Onboarding / login screen |
| `chat` | Chat interface with messages |
| `settings` | Settings menu |
| `voice` | Voice call interface |

## Native Crypto Module

The Signal Protocol implementation is in C++ and shared with the desktop client:

```
app/src/main/cpp/
├── CMakeLists.txt              # Fetches libsignal-protocol-c
├── crypto/
│   ├── crypto_engine.hpp/cpp   # Signal Protocol implementation
│   └── ...
└── jni/
    └── jni_bridge.cpp          # JNI bindings
```

### Crypto Features

- **X3DH** (Extended Triple Diffie-Hellman) — Initial key exchange
- **Double Ratchet** — Forward secrecy for ongoing sessions
- **Sender Keys** — Efficient group encryption
- **Argon2id + XChaCha20-Poly1305** — Identity key encryption at rest

### JNI Interface

```kotlin
// Initialize crypto engine
NativeCrypto.init(store, userId, passphrase)

// Encrypt message
val ciphertext = NativeCrypto.encrypt(recipientId, plaintext.toByteArray())

// Decrypt message
val plaintext = NativeCrypto.decrypt(senderId, recipientId, ciphertext, type)

// Get safety number for verification
val safetyNumber = NativeCrypto.safetyNumber(peerId)
```

## Theming

Three theme options available in **Settings → Appearance → Theme**:

| Option | Description |
|--------|-------------|
| System default | Follows phone's theme setting (default) |
| Light | Clean light theme |
| Dark | Tokyo Night dark theme |

Theme state is managed by `ThemeViewModel` and persisted in DataStore.

## Project Structure

```
app/src/main/java/fi/grotto/android/
├── MainActivity.kt              # Entry point, theme application
├── GrottoApp.kt                 # Application class
│
├── data/
│   ├── local/                   # Room database
│   ├── remote/                  # Network layer
│   └── repository/              # Repositories
│       └── CryptoRepository.kt  # High-level crypto API
│
├── native/                      # JNI bridge
│   ├── NativeCrypto.kt          # JNI interface
│   └── NativeStore.kt           # Room-backed crypto store
│
├── ui/
│   ├── theme/                   # Theme system
│   │   ├── ThemeViewModel.kt    # Theme state management
│   │   └── Color.kt             # Light + Dark colors
│   │
│   └── screen/
│       ├── channels/            # Channel list UI
│       ├── chat/                # Chat UI
│       ├── settings/            # Settings UI
│       └── screenshot/          # Screenshot scenes (debug)
│
└── di/                          # Hilt modules
```

## Security

- **Server sees**: IP addresses, who messages whom, timestamps
- **Server does NOT see**: Message content (E2E encrypted)
- **Identity keys**: Ed25519, encrypted at rest with Argon2id
- **Sessions**: Double Ratchet provides forward secrecy
- **Database**: SQLCipher with 256-bit key from Android Keystore
- **Biometric**: Optional fingerprint unlock for identity key
- **Screen capture**: FLAG_SECURE option in settings

## Wire Protocol

Length-prefixed Protobuf frames over TLS/TCP:

```
┌──────────────┬────────────────────────┐
│  4 bytes     │  N bytes               │
│  (uint32 BE) │  Protobuf Envelope     │
└──────────────┴────────────────────────┘
```

Max message size: **64 KB**

## IRC Commands

| Command | Description |
|---------|-------------|
| `/join #channel` | Join a channel |
| `/part` | Leave current channel |
| `/msg <user> <message>` | Send private message |
| `/me <action>` | Action message |
| `/nick <new_nick>` | Change nickname |
| `/call <user>` | Start voice call |
| `/hangup` | End voice call |
| `/quit` | Disconnect |

## Documentation

- [Architecture (Finnish)](docs/android/architecture.md) — Detailed architecture docs
- [CLAUDE.md](CLAUDE.md) — Claude Code context and guidance

## Related Projects

- [grotto-server](../grotto-server) — C++ relay server
- [grotto-client](../grotto-client) — Desktop terminal client
- [grotto-plugin](../grotto-plugin) — Plugin system

## License

MIT License — see LICENSE file for details.

## Acknowledgments

- **Signal** — Signal Protocol implementation
- **libsignal-protocol-c** — Open Source Signal Protocol library
- **libsodium** — Modern cryptographic library
- **Jetpack Compose** — Android UI toolkit
