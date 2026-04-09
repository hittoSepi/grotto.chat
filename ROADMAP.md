# Grotto Client Release Roadmap

Tämä dokumentti listaa mitä tarvitaan grotto-chat-clientin julkaisuversion saavuttamiseksi.

## Status: Beta → Release Candidate

Viimeisin päivitys: 2026-04-09

---

## 🚫 Blockers (MUST HAVE ennen julkaisua)

### 1. Stability & Error Handling
**Status:** 🚧 Keskeneräinen  
**Tärkeys:** Kriittinen

- [ ] **Connection resilience** - Yhteyden katkeamisen käsittely
  - Automatic reconnect with exponential backoff
  - Pending message queue
  - Connection state indication UI:ssa

- [ ] **Graceful shutdown** - Resurssien vapauttaminen
  - Voice engine cleanup
  - Database flush
  - Network socket close

- [ ] **Crypto state recovery** - Virhetilanteiden korjaus
  - Session desync käsittely
  - Re-keying mekanismi
  - Clear instructions käyttäjälle

---

### 2. Testing & Quality Assurance
**Status:** 🚧 Keskeneräinen  
**Tärkeys:** Kriittinen

- [ ] **Windows build testaus** (Win 10, Win 11)
- [ ] **Linux build testaus** (Ubuntu 22.04+, Arch)
- [ ] **Yhteensopivuustestit** Android-clientin kanssa
- [ ] **Memory leak check** (Valgrind / AddressSanitizer)
- [ ] **Thread safety audit**
- [ ] **Error scenarios** - Mitä tapahtuu kun server ei vastaa, verkko katkeaa, etc.

---

## ✅ Valmis (DONE)

| Ominaisuus | Status | Tiedosto |
|------------|--------|----------|
| E2E Encryption (Signal Protocol) | ✅ | `src/crypto/` |
| Channel messaging | ✅ | `src/net/message_handler.cpp` |
| DM/Private messages | ✅ | `src/net/message_handler.cpp` |
| File transfers | ✅ | `src/file/file_transfer.cpp` |
| Mouse support | ✅ | `src/ui/mouse_support.cpp` |
| Link previews | ✅ | `src/preview/link_previewer.cpp` |
| IRC commands | ✅ | `src/input/command_parser.cpp` |
| Settings UI | ✅ | `src/ui/settings_screen.cpp` |
| Themes (Tokyo Night) | ✅ | `src/ui/color_scheme.hpp` |
| Clipboard integration | ✅ | `src/input/input_handler.cpp` |
| Text selection | ✅ | `src/ui/message_view.cpp` |
| Tab completion | ✅ | `src/input/tab_complete.cpp` |
| Certificate pinning | ✅ | `src/net/net_client.cpp` |
| Rate limiting (client-side) | ✅ | `src/net/net_client.cpp` |
| Help system | ✅ | `src/help/help_manager.cpp` |
| **Voice P2P calls** | ✅ | `src/voice/voice_engine.cpp` |
| **Group Voice** | ✅ | `src/voice/voice_engine.cpp` |

**Voice ominaisuudet:**
- `/call <user>` - P2P puhelut
- `/vmode` - PTT / VOX -tilat
- Speaking indicators
- Mute/deafen
- ICE/TURN tuki konfiguroitavissa
- Noise suppression (WebRTC NS)
- Limiter & jitter buffer

---

## 🎯 Nice to Have (Post-Release)

### UI/UX Parannukset
- [ ] **UI Scaling** - `Ctrl + +/-` zoomaus
- [ ] **Window management** - /window komento ikkunoiden hallintaan
- [ ] **Message search** - Kanavien sisäinen hakutoiminto
- [ ] **Notification settings** - Hienovaraisempi kontrolli

### Voice Parannukset
- [ ] **Audio echo test** - Input testi ennen callia
- [ ] **Per-user volume control** - Yksittäisten käyttäjien volyymi
- [ ] **Call quality indicators** - Latency/packet loss näyttö
- [ ] **Voice recording** - Puheluiden nauhoitus (opt-in)

### Integraatiot
- [ ] **System notifications** - Native OS notifikaatiot
- [ ] **URL scheme handler** - `ircord://` linkkien avaus
- [ ] **Auto-updater** - Ominainen päivitysmekanismi

---

## 📋 Release Checklist

### Ennen julkaisua:
- [ ] Kaikki blockerit valmiita
- [ ] Windows build testattu (Win 10, Win 11)
- [ ] Linux build testattu (Ubuntu 22.04+, Arch)
- [ ] Yhteensopivuustesti Android-clientin kanssa
- [ ] Yhteensopivuustesti server-version kanssa
- [ ] Memory leak check (Valgrind/AddressSanitizer)
- [ ] Thread safety audit

### Dokumentaatio:
- [ ] README.md päivitetty
- [ ] CHANGELOG.md päivitetty
- [ ] Asennusohjeet Windowsille
- [ ] Asennusohjeet Linuxille
- [ ] Troubleshooting-ohjeet

### Jako:
- [ ] Windows installer (.msi tai .exe)
- [ ] Linux paketit (.deb, .AppImage tai repo)
- [ ] Release GitHubissa
- [ ] Checksums (SHA-256) julkaistu

---

## 🐛 Tunnetut Bugit

Katso täydellinen lista: [BUGS.md](./grotto-chat-client/BUGS.md)

Korjatut:
- ✅ DM-kanavan userlist -ongelma
- ✅ Kuvapreview ei näy muille
- ✅ Offline-viestit
- ✅ Server-tabin userlist piilotettu
- ✅ Private message decryptaus
- ✅ Voice P2P/Group - toiminnallisuus

Avoimet / Tarkistettava:
- 🚧 Long-running session stability
- 🚧 Graceful shutdown edge cases

---

## 📊 Arvioitu Aikataulu

| Milestone | Arvio | Blockerit |
|-----------|-------|-----------|
| Stability fixes | 1 vk | Connection resilience, shutdown |
| Testing & QA | 1 vk | Windows/Linux testaus |
| RC1 Release | 2 vk yht. | Kaikki yllä |
| Final Release | +1 vk (feedback) | RC1 testaus |

**Arvioitu julkaisu:** Huhtikuu-Lokakuu 2026

---

## 🔧 Kehitysprioriteet tällä hetkellä

1. **Connection Resilience** - Reconnect logiikka ja error recovery
2. **Testing** - Manuaalitestaus eri verkkoympäristöissä ja buildit
3. **Documentation** - Käyttöohjeet ja troubleshooting
4. **Release packaging** - Installerit ja jakelupaketit

---

*Päivitä tämä dokumentti kun status muuttuu!*
