# Grotto Android Client — Arkkitehtuuri

**Stack:** Kotlin · Jetpack Compose · C++ (NDK) · Protobuf · Room · Hilt

---

## 1. Komponentit yhdellä silmäyksellä

```
┌───────────────────────────────────────────────────────────────────┐
│                     grotto-android                                 │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │                    UI Layer (Compose)                        │ │
│  │   Screens · Components · Theme · Navigation                 │ │
│  └────────────────────────────┬────────────────────────────────┘ │
│                               │                                   │
│  ┌────────────────────────────┴────────────────────────────────┐ │
│  │              ViewModel Layer (Kotlin)                        │ │
│  │   ChatVM · VoiceVM · SettingsVM · ChannelListVM             │ │
│  │   ThemeViewModel (NEW)                                      │ │
│  └────────────────────────────┬────────────────────────────────┘ │
│                               │                                   │
│  ┌────────────────────────────┴────────────────────────────────┐ │
│  │            Repository / UseCase Layer (Kotlin)               │ │
│  │   MessageRepo · ChannelRepo · KeyRepo · VoiceRepo           │ │
│  │   CryptoRepository (NEW) · NativeStore (NEW)                │ │
│  └──────┬─────────────────┬─────────────────┬──────────────────┘ │
│         │                 │                 │                     │
│  ┌──────┴──────┐  ┌───────┴───────┐  ┌─────┴──────────────────┐ │
│  │  NetClient  │  │  LocalStore   │  │  Native Bridge (JNI)   │ │
│  │  (OkHttp /  │  │  (Room DB)    │  │  ┌──────────────────┐  │ │
│  │   raw TLS   │  │  + SharedPrefs│  │  │  CryptoEngine    │  │ │
│  │   socket)   │  │  + FCM Token  │  │  │  (libsignal +    │  │ │
│  │   FCM       │  │  + Cert Pins  │  │  │   libsodium)     │  │ │
│  │  + CertPin  │  └───────────────┘  │  │  VoiceEngine     │  │ │
│  └─────────────┘                     │  │  Argon2id +      │  │ │
│                                       │  │  XChaCha20-P1305 │  │ │
│                                       │  ├──────────────────┤  │ │
│                                       │  │  Group Cipher    │  │ │
│                                       │  │  (Sender Keys)   │  │ │
│                                       │  └──────────────────┘  │ │
│                                       └────────────────────────┘ │
└───────────────────────────────────────────────────────────────────┘
```

### Miksi Kotlin + NDK (C++) hybridimalli?

Grotto-serveri ja desktop-client ovat C++20. Signal Protocol -sessiot ja krypto-opsit
on kirjoitettu C++:lla ja jaetaan desktop-clientin kanssa. Android-client käyttää
**samaa natiivikerrosta** — JNI-bridge yhdistää Kotlin-UI:n C++-krypto-moottoriin.

| Kerros | Kieli | Rationale |
|--------|-------|-----------|
| UI + ViewModel | Kotlin | Compose on paras Android UI toolkit, lifecycle-tuki |
| Repo / UseCase | Kotlin | Coroutines, Room, Hilt — Android-ekosysteemi |
| Network framing | Kotlin | OkHttp/raw socket + Protobuf-lite, Android-natiivi TLS |
| Crypto (Signal) | C++ (NDK) | **Jaettu desktop-clientin kanssa**, libsignal-protocol-c |
| Voice | C++ (NDK) | Jaettu desktop-clientin kanssa, libdatachannel + Opus |
| Audio I/O | C++ (NDK) | Oboe (Googlen low-latency Android audio API) |

---

## 2. Thread Model

```
┌─────────────────────────────────────────────────────────────┐
│  Main Thread (UI)                                            │
│   Compose recomposition, user input, navigation              │
│   → EI koskaan blokkaa — kaikki raskas työ coroutineissa    │
└────────────────────┬────────────────────────────────────────┘
                     │ StateFlow / SharedFlow
         ┌───────────┼───────────────┬──────────────────┐
         ▼           ▼               ▼                  ▼
┌──────────────┐ ┌──────────┐ ┌───────────────┐ ┌────────────┐
│  IO Dispatcher│ │ Default  │ │  NDK Thread   │ │ Audio      │
│  (Coroutine) │ │Dispatcher│ │  Pool         │ │ Thread     │
│              │ │          │ │  (C++ side)   │ │ (Oboe      │
│ - TLS socket │ │ - Proto  │ │              │ │  callback, │
│ - Room DB    │ │   parse  │ │ - Signal ops │ │  realtime) │
│ - Reconnect  │ │ - State  │ │ - ICE/DTLS   │ │            │
│              │ │   update │ │ - Opus enc/  │ │ - Capture  │
│              │ │          │ │   decode     │ │ - Playback │
└──────────────┘ └──────────┘ └───────────────┘ └────────────┘
```

**Kriittiset säännöt:**

1. **UI Thread ei koskaan blokkaa** — kaikki I/O ja krypto coroutineissa
2. **NDK-kutsut ovat kalliita** — JNI boundary overhead ~1-5µs per call, batchaa kun mahdollista
3. **Audio thread (Oboe callback)** on realtime — ei allokointeja, ei lockkeja, ei JNI-kutsuja
4. **Signal Protocol -operaatiot** voivat kestää 1-10ms (X3DH erityisesti) — aina Default dispatcherissa

---

## 3. JNI Bridge — Native Crypto Layer

### Toteutetut C++ komponentit (src/main/cpp/)

```
grotto-android/app/src/main/cpp/
├── CMakeLists.txt                    # FetchContent: libsignal-protocol-c
├── crypto/
│   ├── crypto_engine.hpp/.cpp        # Täysi Signal Protocol -toteutus
│   │                                    - X3DH avaintenvaihto
│   │                                    - Double Ratchet (1:1 viestit)
│   │                                    - Sender Keys (ryhmäviestit)
│   │                                    - Argon2id + XChaCha20-Poly1305
│   ├── identity.cpp                  # Identity key management
│   ├── signal_store.cpp              # Signal Protocol store interface
│   └── group_session.cpp             # Group/Sender key sessions
└── jni/
    └── jni_bridge.cpp                # JNI-toteutus NativeCrypto:lle
```

### JNI Interface (Kotlin-puoli)

```kotlin
object NativeCrypto {
    init { System.loadLibrary("grotto-native") }

    // Identity & Registration
    external fun identityPub(): ByteArray?           // Ed25519 public key
    external fun currentSpk(): SpkInfo?              // Signed pre-key
    external fun prepareRegistration(numOpks: Int): ByteArray?  // KeyUpload

    // 1:1 Encryption (Double Ratchet)
    external fun encrypt(recipientId: String, plaintext: ByteArray): ByteArray?
    external fun decrypt(senderId: String, recipientId: String, 
                        ciphertext: ByteArray, type: Int): ByteArray?
    external fun onKeyBundle(recipientId: String, bundleData: ByteArray)
    external fun hasSession(recipientId: String): Boolean

    // Group Encryption (Sender Keys)
    external fun initGroupSession(channelId: String, members: Array<String>)
    external fun encryptGroup(channelId: String, plaintext: ByteArray): ByteArray?
    external fun decryptGroup(senderId: String, channelId: String, 
                             ciphertext: ByteArray, skdm: ByteArray?): ByteArray?
    external fun processSenderKeyDistribution(senderId: String, 
                                             channelId: String, skdm: ByteArray)

    // Safety Number & Auth
    external fun safetyNumber(peerId: String): String
    external fun signChallenge(nonce: ByteArray): ByteArray?

    // Storage interface (JNI callback)
    interface Store {
        fun saveIdentity(userId: String, pubKey: ByteArray, 
                        privKeyEncrypted: ByteArray, salt: ByteArray)
        fun loadIdentity(userId: String): ByteArray?
        fun saveSession(address: String, record: ByteArray)
        fun loadSession(address: String): ByteArray?
        fun savePreKey(id: Int, record: ByteArray)
        fun loadPreKey(id: Int): ByteArray?
        fun removePreKey(id: Int)
        fun saveSignedPreKey(id: Int, record: ByteArray)
        fun loadSignedPreKey(id: Int): ByteArray?
        fun savePeerIdentity(userId: String, pubKey: ByteArray)
        fun loadPeerIdentity(userId: String): ByteArray?
        fun saveSenderKey(senderKeyId: String, record: ByteArray)
        fun loadSenderKey(senderKeyId: String): ByteArray?
    }
}
```

### NativeStore — Room-backed JNI Store

`NativeStore` toteuttaa `NativeCrypto.Store` -rajapinnan ja välittää
tallennetukset Room-tietokantaan ja EncryptedSharedPreferences:iin.

```kotlin
class NativeStore(context: Context, database: GrottoDatabase) : NativeCrypto.Store {
    // Identity, sessions, pre-keys → SharedPreferences (salattu)
    // Peer identities → Room DB (PeerIdentityEntity)
}
```

---

## 3.5 JNI Bridge — Native Voice Layer (NEW)

### VoiceEngine C++ komponentit

```
grotto-android/app/src/main/cpp/
├── voice/
│   ├── voice_engine.hpp/.cpp       # VoiceEngine foundation
│   │                                    - PeerConnection management
│   │                                    - WebRTC signaling
│   │                                    - Call state machine
│   │                                    - (Stubs: libdatachannel, Opus, Oboe)
│   └── ...                           # TODO: Full implementation
└── jni/
    ├── jni_bridge.cpp                # Crypto JNI bindings
    └── jni_voice_bridge.cpp          # NEW: Voice JNI bindings
```

### JNI Interface (Kotlin-puoli)

```kotlin
object NativeVoice {
    init { System.loadLibrary("grotto-native") }

    // Initialization
    external fun init(sampleRate: Int, framesPerBuffer: Int): Boolean
    external fun destroy()

    // Room/Voice Channel
    external fun joinRoom(channelId: String, isPrivateCall: Boolean)
    external fun leaveRoom()
    external fun isInRoom(): Boolean
    external fun getParticipants(): Array<String>

    // Private Calls
    external fun call(peerId: String)
    external fun acceptCall()
    external fun declineCall()
    external fun hangup()

    // Audio Controls
    external fun setMuted(muted: Boolean)
    external fun setDeafened(deafened: Boolean)

    // Signaling
    external fun onVoiceSignal(fromUser: String, signalType: Int, data: ByteArray)

    // Stats
    external fun getAudioStats(): AudioStats

    // Callbacks to Kotlin
    interface VoiceCallback {
        fun onIceCandidate(peerId: String, candidate: ByteArray)
        fun onPeerJoined(peerId: String)
        fun onPeerLeft(peerId: String)
        fun onAudioLevel(peerId: String, level: Float)
        fun onIncomingCall(peerId: String, channelId: String)
        fun onCallAccepted(peerId: String)
        fun onCallDeclined(peerId: String, reason: String)
        fun onConnectionStateChanged(state: ConnectionState)
    }
}
```

### VoiceRepository

`VoiceRepository` käyttää `NativeVoice` JNI-rajapintaa ja tarjoaa
korkean tason API:n ViewModeleille:

```kotlin
@Singleton
class VoiceRepository @Inject constructor() : NativeVoice.VoiceCallback {
    
    fun joinRoom(channelId: String) { NativeVoice.joinRoom(channelId, false) }
    fun leaveRoom() { NativeVoice.leaveRoom() }
    fun toggleMute() { NativeVoice.setMuted(!isMuted) }
    fun call(peerId: String) { NativeVoice.call(peerId) }
    fun acceptCall() { NativeVoice.acceptCall() }
    
    // Signal processing from server
    fun onVoiceSignal(fromUser: String, type: Int, data: ByteArray) {
        NativeVoice.onVoiceSignal(fromUser, type, data)
    }
    
    // Callbacks implement native → Kotlin events
    override fun onPeerJoined(peerId: String) { ... }
    override fun onPeerLeft(peerId: String) { ... }
    override fun onAudioLevel(peerId: String, level: Float) { ... }
}
```

### Signaalityypit (WebRTC Signaling)

| Type | Arvo | Kuvaus |
|------|------|--------|
| OFFER | 1 | WebRTC offer SDP |
| ANSWER | 2 | WebRTC answer SDP |
| ICE_CANDIDATE | 3 | ICE candidate |
| BYE | 4 | Puhelun lopetus |
| CALL_REQUEST | 5 | Saapuva puhelu |
| CALL_ACCEPT | 6 | Puhelu hyväksytty |
| CALL_DECLINE | 7 | Puhelu hylätty |

### TODO: Täydellinen toteutus

- [ ] **libdatachannel** - WebRTC PeerConnection ja data channel
- [ ] **Opus** - Audio encoding/decoding
- [ ] **Oboe** - Low-latency audio capture/playback
- [ ] **ICE/DTLS-SRTP** - Connection establishment ja encryption

---

## 4. Teema-järjestelmä (NEW)

### ThemeViewModel

```kotlin
@HiltViewModel
class ThemeViewModel @Inject constructor(userPreferences: UserPreferences) {
    val themeMode: StateFlow<String>     // THEME_SYSTEM / THEME_LIGHT / THEME_DARK
    val isDarkTheme: StateFlow<Boolean?> // null = follow system
}
```

### Käyttö MainActivity:ssä

```kotlin
class MainActivity : ComponentActivity() {
    private val themeViewModel: ThemeViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        setContent {
            val isDarkTheme by themeViewModel.isDarkTheme.collectAsStateWithLifecycle()
            
            // isDarkTheme = null → seuraa järjestelmää
            // isDarkTheme = true/false → pakota teema
            GrottoTheme(darkTheme = isDarkTheme ?: isSystemInDarkTheme()) {
                // ...
            }
        }
    }
}
```

### Teema-asetukset

Asetukset → Ulkoasu → Teema:
- **Järjestelmän oletus** (oletus) — Seuraa puhelimen teema-asetusta
- **Vaalea** — Pakota vaalea teema
- **Tumma** — Pakota Tokyo Night tumma teema

Toteutus: `ThemeSelectorDialog.kt`, `SettingsScreen.kt`

---

## 5. Hakemistorakenne (Päivitetty)

```
grotto-android/
├── app/src/main/
│   ├── java/fi/grotto/android/
│   │   ├── MainActivity.kt              # Teeman soveltaminen
│   │   ├── GrottoApp.kt
│   │   │
│   │   ├── data/
│   │   │   ├── local/
│   │   │   │   ├── GrottoDatabase.kt    # v2: publicKey kenttä lisätty
│   │   │   │   ├── Migrations.kt        # NEW: MIGRATION_1_2
│   │   │   │   ├── dao/
│   │   │   │   └── entity/
│   │   │   │       └── PeerIdentityEntity.kt  # publicKey (Base64)
│   │   │   ├── remote/
│   │   │   │   └── ProtobufExt.kt       # NEW: Protobuf helpers
│   │   │   └── repository/
│   │   │       ├── CryptoRepository.kt  # NEW: High-level crypto API
│   │   │       └── VoiceRepository.kt   # Updated: NativeVoice calls
│   │   │
│   │   ├── native/                      # JNI bridge
│   │   │   ├── NativeCrypto.kt          # Päivitetty täysi toteutus
│   │   │   ├── NativeStore.kt           # NEW: Room-backed store
│   │   │   └── NativeVoice.kt           # NEW: Voice engine JNI
│   │   │
│   │   ├── security/                    # Security features
│   │   │   ├── pinning/                 # NEW: Certificate pinning
│   │   │   │   ├── CertificatePin.kt       # Pin data class
│   │   │   │   ├── CertificatePinner.kt    # OkHttp interceptor
│   │   │   │   └── PinRepository.kt        # Pin storage
│   │   │   ├── database/                # NEW: Database encryption
│   │   │   │   └── DatabaseEncryptionManager.kt  # Key management
│   │   │   └── biometric/               # NEW: Biometric auth
│   │   │       ├── BiometricAuthManager.kt     # Auth manager
│   │   │       ├── BiometricCryptoManager.kt   # Crypto wrapper
│   │   │       └── BiometricPrompt.kt          # UI component
│   │   │
│   │   ├── fcm/                         # NEW: Firebase Cloud Messaging
│   │   │   ├── GrottoMessagingService.kt   # FCM message handler
│   │   │   ├── GrottoInstanceIdService.kt  # Token refresh
│   │   │   └── FcmRepository.kt            # Token management
│   │   │
│   │   ├── ui/
│   │   │   ├── theme/
│   │   │   │   ├── ThemeViewModel.kt    # NEW: Teeman hallinta
│   │   │   │   ├── Color.kt             # Light + Dark (Tokyo Night)
│   │   │   │   └── ...
│   │   │   └── screen/
│   │   │       ├── settings/
│   │   │       │   ├── SettingsScreen.kt
│   │   │       │   ├── ThemeSelectorDialog.kt      # NEW: Teeman valinta
│   │   │       │   ├── CertificatePinningScreen.kt # NEW: Cert pinning UI
│   │   │       │   └── CertificatePinningViewModel.kt
│   │   │       └── notifications/
│   │   │           ├── NotificationSettingsScreen.kt  # NEW: FCM settings
│   │   │           └── NotificationSettingsViewModel.kt
│   │   │
│   │   └── di/
│   │       └── DatabaseModule.kt        # Päivitetty: NativeStore provider
│   │
│   ├── cpp/                             # NDK native code
│   │   ├── CMakeLists.txt               # libsignal-protocol-c FetchContent
│   │   ├── crypto/
│   │   │   ├── crypto_engine.hpp/.cpp   # Täysi Signal Protocol
│   │   │   └── ...
│   │   ├── voice/
│   │   │   ├── voice_engine.hpp/.cpp    # NEW: VoiceEngine foundation
│   │   │   └── ...                      # TODO: Full WebRTC impl
│   │   └── jni/
│   │       ├── jni_bridge.cpp           # Crypto JNI bindings
│   │       └── jni_voice_bridge.cpp     # NEW: Voice JNI bindings
│   │
│   └── proto/
│       └── grotto.proto                 # Jaettu .proto
```

---

## 6. Dataflow: Viestin lähetys (Päivitetty)

```
User painaa Send
  → ChatViewModel.sendMessage(text)
    → CryptoRepository.encrypt(recipientId, text)
      → NativeCrypto.encrypt(recipientId, bytes) [JNI]
        └── [C++ CryptoEngine::encrypt()]
            ├── Jos sessio olemassa → Double Ratchet encrypt
            └── Jos ei → return null (pyydä avainniput)
      ← Palaa ciphertext tai null
    
    Jos ciphertext == null:
      → Pyydä serveriltä recipientin KeyBundle
      → CryptoRepository.processKeyBundle(bundle)
        → NativeCrypto.onKeyBundle(recipientId, bundle) [JNI]
          └── [C++ X3DH session establishment]
      → Yritä encrypt uudelleen
    
    → Envelope(type=CHAT, payload=ciphertext)
    → GrottoSocket.send(envelope) [TLS write]
    → MessageRepository.insertLocal(msg, status=SENDING)
    → UI: MessageBubble shows ⏳

Server relay → recipient
  → GrottoSocket.receive() [suspend, IO dispatcher]
    → FrameCodec.decode() → Envelope
    → MessageHandler.dispatch(envelope)
      → CryptoRepository.decrypt(senderId, recipientId, ct, type)
        → NativeCrypto.decrypt(senderId, recipientId, ct, type) [JNI]
          └── [C++ CryptoEngine::decrypt()]
              ├── type=2 (SIGNAL_MESSAGE) → Double Ratchet decrypt
              └── type=3 (PREKEY_MESSAGE) → X3DH initial decrypt
      → MessageRepository.insert(decryptedMsg)
      → NotificationManager (jos app ei ole foreground)
      → ChatViewModel.messages StateFlow päivittyy
        → Compose recomposition → uusi viesti näkyy
```

---

## 7. Turvallisuus — Päivitetyt tiedot

| Uhka | Suojaus |
|------|---------|
| Root/jailbreak | Detect & warn, mutta älä estä (kaveriporukka) |
| Screen capture | FLAG_SECURE chat-näkymässä (konfiguroitava Settingsissä) |
| Identity key extraction | Argon2id + XChaCha20-Poly1305, Android Keystore hardware-backed |
| Session state | SharedPreferences + EncryptedSharedPreferences |
| Database encryption | SQLCipher, 256-bit key, Android Keystore backed |
| Identity key access | Optional biometric auth (fingerprint/face) |
| Network intercept | Certificate pinning (OkHttp + custom pinner) |
| Backup leak | `android:allowBackup="false"` |
| Clipboard snoop | Clear clipboard 30s after Safety Number copy |
| Push content leak | FCM: vain "wakeup" — ei viestin sisältöä |

### Identity Key Encryption

```cpp
// C++: crypto_engine.cpp
std::vector<uint8_t> CryptoEngine::encryptIdentityPriv(
    const std::array<uint8_t, 64>& priv_key,
    const std::string& passphrase,
    std::vector<uint8_t>& salt_out) {
    
    // 1. Argon2id key derivation (libsodium)
    crypto_pwhash(key, passphrase, salt, 
                  OPSLIMIT_INTERACTIVE, MEMLIMIT_INTERACTIVE,
                  crypto_pwhash_ALG_ARGON2ID13)
    
    // 2. XChaCha20-Poly1305 encryption
    crypto_aead_xchacha20poly1305_ietf_encrypt(
        plaintext=priv_key, key=derived_key, nonce=random)
}
```

### Certificate Pinning

Grotto Android käyttää **certificate pinning** -tekniikkaa varmistaakseen, 
että sovellus kommunikoi vain oikean serverin kanssa.

```
┌─────────────────────────────────────────────────────────────┐
│                    Certificate Pinning                      │
│                                                             │
│  1. Admin extractaa serverin sertifikaatin julkisen avaimen │
│  2. Pin (SHA-256 hash) lisätään sovellukseen                │
│  3. Yhteydenotossa verrataan serverin pinniä konfiguroituun │
│  4. Jos ei match → yhteys katkaistaan (MITM-suoja)          │
│                                                             │
│  Backup pin: Sallii sertifikaatin uusimisen                 │
└─────────────────────────────────────────────────────────────┘
```

#### Toteutus

```kotlin
// security/pinning/CertificatePin.kt
data class CertificatePin(
    val pattern: String,        // "*.example.com"
    val pin: String,            // Base64 SHA-256 hash
    val isBackupPin: Boolean,   // Varmuuskopio uudelle sertifikaatille
)

// security/pinning/CertificatePinner.kt
class CertificatePinner(
    config: PinningConfig,
    onPinFailure: ((hostname, peerPins) -> Unit)?
) : Interceptor {
    override fun intercept(chain: Chain): Response {
        val response = chain.proceed(chain.request())
        // Validoi sertifikaatti
        if (!validatePin(chain.request().url.host, certificate)) {
            throw SSLPeerUnverifiedException("Pin mismatch")
        }
        return response
    }
}

// security/pinning/PinRepository.kt — Tallennus DataStoreen
class PinRepository {
    suspend fun addPin(hostname: String, pin: String)
    suspend fun removePin(hostname: String, pin: String)
    val pinningConfig: Flow<PinningConfig>
}
```

#### Käyttöliittymä

Asetukset → Turvallisuus → Certificate Pinning:
- Näytä konfiguroidut pinnit
- Lisää uusi pin (hostname + SHA-256 hash)
- Merkitse backup-pin
- Poista vanhat pinnit

#### Pinin generointi serveriltä

```bash
# Extract public key from certificate
openssl x509 -in server.crt -pubkey -noout | \
  openssl pkey -pubin -outform der | \
  openssl dgst -sha256 -binary | \
  openssl enc -base64

# Output: rE2/zFR1L5z0U4Y8Jq/x8O3K8n9...
```

### Database Encryption (SQLCipher)

Grotto salaa Room-tietokannan SQLCipher-kirjastolla, jotta viestit ja 
muut arkaluonteiset tiedot ovat suojattuja laitteella (at-rest encryption).

#### Arkkitehtuuri

```
┌─────────────────────────────────────────────────────────────┐
│                    SQLCipher Encryption                     │
│                                                             │
│  1. Satunnainen 256-bittinen avain generoidaan              │
│  2. Avain salataan Android Keystoren master-avaimella       │
│  3. Salattu avain tallennetaan EncryptedSharedPreferencesiin│
│  4. SQLCipher käyttää avainta tietokannan salaukseen        │
│  5. Ilman avainta tietokanta on lukukelvoton                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

#### Toteutus

```kotlin
// security/database/DatabaseEncryptionManager.kt
class DatabaseEncryptionManager(context: Context) {
    
    // Hae tai luo salausavio
    suspend fun getOrCreateKey(): String {
        // 1. Tarkista onko avain jo olemassa
        // 2. Jos ei → generoi 256-bittinen satunnainen avain
        // 3. Salaa avain Android Keystoren avaimella
        // 4. Tallenna EncryptedSharedPreferencesiin
    }
    
    private fun generateRandomKey(): String {
        // 32 tavua (256 bittiä) hex-muodossa
        return secureRandomBytes(32).toHexString()
    }
}

// data/local/EncryptedDatabaseFactory.kt
object EncryptedDatabaseFactory {
    fun <T : RoomDatabase> create(context, klass, name): T {
        val key = DatabaseEncryptionManager(context).getOrCreateKey()
        val passphrase = hexToBytes(key)
        
        // SQLCipher factory
        val factory = SupportFactory(passphrase)
        
        return Room.databaseBuilder(context, klass, name)
            .openHelperFactory(factory)  // <-- SQLCipher
            .build()
    }
}
```

#### Tietoturvaominaisuudet

| Ominaisuus | Toteutus |
|------------|----------|
| **Avaimen pituus** | 256 bittiä (AES-256) |
| **Avaimen tallennus** | Android Keystore + EncryptedSharedPreferences |
| **Hardware-backed** | Käytössä kun saatavilla (TEE/SE) |
| **Avaimen kierto** | Mahdollista (vaatii tietokannan uudelleensalauksen) |
| **Biometrinen lukitus** | Tuettu (asetettavissa MasterKey:ssä) |

#### Rakenne

```
security/
└── database/
    └── DatabaseEncryptionManager.kt    # Avaimen hallinta
data/
├── local/
│   ├── EncryptedDatabaseFactory.kt    # SQLCipher-factory
│   └── GrottoDatabase.kt              # Room-database
└── di/
    └── DatabaseModule.kt              # Hilt-moduuli
```

#### Käyttö

```kotlin
// DatabaseModule.kt — Automaginen salaus
@Provides
@Singleton
fun provideDatabase(@ApplicationContext context: Context): GrottoDatabase {
    return EncryptedDatabaseFactory.create(
        context = context,
        klass = GrottoDatabase::class.java,
        name = "grotto.db"
    )
}
```

Tietokanta on nyt täysin salattu — ilman oikeaa avainta tiedostoa ei voi lukea.

### Biometrinen autentikointi (Identity Key)

Grotto tukee valinnaista **biometristä autentikointia** identiteettiavaimen 
käytölle. Tämä vaatii käyttäjältä sormenjälki- tai kasvotunnistuksen ennen 
kryptografisia operaatioita.

#### Toiminta

```
┌─────────────────────────────────────────────────────────────┐
│                 Biometric Authentication                    │
│                                                             │
│  1. Käyttäjä avaa sovelluksen                               │
│  2. Tarkistetaan onko biometrinen auth käytössä             │
│  3. Jos käytössä → näytetään BiometricPrompt                │
│  4. Käyttäjä tunnistautuu sormenjälkillä/kasvoilla          │
│  5. Onnistuessa → avataan identiteettiavain                 │
│  6. Epäonnistuessa → estetään pääsy                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

#### Toteutus

```kotlin
// security/biometric/BiometricAuthManager.kt
class BiometricAuthManager(context: Context) {
    
    // Tarkista saatavuus
    fun canAuthenticate(): BiometricAvailability
    
    // Suorita autentikointi
    suspend fun authenticate(
        activity: FragmentActivity,
        title: String,
        subtitle: String,
    ): Boolean
    
    // Käyttäjän asetus
    fun isBiometricEnabled(): Boolean
    fun setBiometricEnabled(enabled: Boolean)
}

// security/biometric/BiometricCryptoManager.kt
class BiometricCryptoManager(
    private val biometricAuthManager: BiometricAuthManager,
) {
    // Suorita krypto-operaatio biometrisellä suojauksella
    suspend fun <T> performWithBiometric(
        activity: FragmentActivity,
        operation: suspend () -> T,
    ): T?
    
    // Erikoistapaukset
    suspend fun initializeWithBiometric(...): Boolean
    suspend fun decryptWithBiometric(...): ByteArray?
    suspend fun signWithBiometric(...): ByteArray?
}
```

#### Turvallisuusominaisuudet

| Ominaisuus | Toteutus |
|------------|----------|
| **Biometrinen data** | Ei koskaan poistu laitteesta |
| **Tallennus** | Android Keystore hardware-backed |
| **Varmuus** | Strong biometric (Class 3) vaaditaan |
| **Fallback** | Salasana aina vaihtoehtona |

#### Käyttöliittymä

Asetukset → Turvallisuus → Biometrinen autentikointi:
- Ota käyttöön/poista käytöstä
- Testaa toimivuus
- Näytä status

---

## 8. Build & NDK Setup

```kotlin
// app/build.gradle.kts
android {
    defaultConfig {
        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++20"
                arguments += "-DANDROID_STL=c++_shared"
                abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}
```

### NDK CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.22.1)
project(grotto-native)

set(CMAKE_CXX_STANDARD 20)

# Fetch libsignal-protocol-c
FetchContent_Declare(
    libsignal-protocol-c
    GIT_REPOSITORY https://github.com/signalapp/libsignal-protocol-c.git
    GIT_TAG v2.3.3
)
FetchContent_MakeAvailable(libsignal-protocol-c)

add_library(grotto-native SHARED
    jni/jni_bridge.cpp
    crypto/crypto_engine.cpp
    crypto/identity.cpp
    crypto/signal_store.cpp
    crypto/group_session.cpp
)

target_link_libraries(grotto-native
    signal-protocol-c    # Fetched
    crypto               # OpenSSL from NDK
    sodium               # libsodium
    log                  # Android log
)
```

---

## 9. Tietokantamuutokset

### Migration 1 → 2

```kotlin
val MIGRATION_1_2 = object : Migration(1, 2) {
    override fun migrate(db: SupportSQLiteDatabase) {
        // Lisätty publicKey kenttä PeerIdentityEntityyn
        // NativeStore tallentaa Base64-koodatun avaimen tähän
        db.execSQL("ALTER TABLE peer_identities ADD COLUMN public_key TEXT DEFAULT NULL")
    }
}
```

---

## 10. Push Notifications (FCM) — NEW

### Arkkitehtuuri

```
┌─────────────────────────────────────────────────────────────┐
│                    Firebase Cloud Messaging                  │
│                         (Google)                             │
└──────────────┬──────────────────────────────────────────────┘
               │ FCM Token, Push payload
               ▼
┌─────────────────────────────────────────────────────────────┐
│              GrottoMessagingService                          │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  onMessageReceived()                                  │  │
│  │  ├── "wakeup" → trigger background sync               │  │
│  │  ├── "message" → show notification (no content)       │  │
│  │  ├── "call" → show incoming call notification         │  │
│  │  └── "channel_invite" → show invite notification      │  │
│  └──────────────────────────────────────────────────────┘  │
└──────────────┬──────────────────────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────────────────────┐
│                    FcmRepository                             │
│  - Token registration/unregistration                        │
│  - Local token storage (DataStore)                          │
│  - Notification settings management                         │
│  - App lifecycle tracking (foreground/background)           │
└─────────────────────────────────────────────────────────────┘
```

### Tietoturva & Yksityisyys

| Ominaisuus | Toteutus |
|------------|----------|
| Viestin sisältö | **Ei lähetetä** FCM:ssä — vain "wakeup"-signaali |
| Notification preview | Vain lähettäjä ja kanava, ei viestin sisältöä |
| Token storage | Local DataStore, rekisteröity serverille autentikoinnin jälkeen |
| Token refresh | Automattinen, GrottoMessagingService hoitaa |

### Protokollaviestit

```protobuf
// Client → Server: Register FCM token
message FcmTokenRegistration {
  string token = 1;        // FCM registration token
  string platform = 2;     // "android"
  string device_name = 3;  // Optional device identifier
}

// Client → Server: Unregister token
message FcmUnregister {
  string token = 1;        // Token to unregister
}
```

### Käyttöliittymä

`NotificationSettingsScreen.kt` — Asetukset → Ilmoitukset:
- Push-notifikaatiot päälle/pois
- Maininta-notifikaatiot (@username)
- Puhelu-notifikaatiot
- Android 13+: Runtime permission käsittely

---

## 11. TODO / Tulevat parannukset

- [~] VoiceEngine JNI-toteutus (foundation valmis, libdatachannel + Opus + Oboe jäljellä)
- [x] FCM push-notifikaatiot offline-viesteille
- [x] Certificate pinning tuotantoserverille
- [x] SQLCipher Room-tietokannan salaamiseen
- [x] Biometric auth identity-avaimelle (optionaalinen)
