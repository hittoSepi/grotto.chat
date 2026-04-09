# Grotto Client Release Roadmap

Tämä dokumentti listaa mitä tarvitaan `grotto-chat-client`-julkaisuversion saavuttamiseksi nykykoodin perusteella.

## Status: Beta -> Release Candidate

Viimeisin paivitys: 2026-04-09

---

## Release Blockers

### 1. Stability & Error Handling
Status: 🚧 Keskenerainen  
Tarkeys: Kriittinen

- [x] **Connection resilience**
  - Auto reconnect exponential backoffilla
  - `auto_reconnect`-asetuksen kunnioitus
  - Reconnect/auth-safe outbound queue viesteille
  - Yhteyden tila ja jonossa olevat lahetykset status barissa

- [x] **Graceful shutdown**
  - Yksi keskitetty app shutdown-polku
  - Voice/preview/network/file cleanup samassa jarjestyksessa
  - Socket close + reconnect stop ilman roikkuvia reunatapauksia
  - `/quit`, UI-exit ja logout kulkevat saman sulkupolun kautta

- [ ] **Crypto state recovery**
  - Session desync -tapaukset loppuun asti
  - Re-keying ja identity reset -virta hiotuksi
  - Selkeat kayttajaviestit kun secure session korjataan

### 2. Testing & QA
Status: 🚧 Keskenerainen  
Tarkeys: Kriittinen

- [ ] Windows build testaus (Win 10, Win 11)
- [ ] Linux build testaus (Ubuntu 22.04+, Arch)
- [ ] Yhteensopivuustestit Android-clientin kanssa
- [ ] Memory leak check (ASan / Valgrind)
- [ ] Thread safety audit
- [~] Error-scenario QA
  - [x] Repeatable QA harness / checklist target (`qa-error-scenarios`)
  - [~] Server restart / reconnect ajettu Windows release-ehdokkaalla
  - [ ] Shutdown / transfer -skenaariot viela ajamatta release-ehdokkaalla
  - [ ] Aja sama polku Windows- ja Linux-buildilla

### 3. Release Packaging & Docs
Status: 🚧 Keskenerainen  
Tarkeys: Korkea

- [ ] Windows installer (.msi tai .exe)
- [ ] Linux paketit (.deb, .AppImage tai repo)
- [ ] Julkaisudokumentaatio ja troubleshooting
- [ ] CHANGELOG / release notes

---

## Done

| Ominaisuus | Status | Sijainti |
|------------|--------|----------|
| E2E encryption (Signal Protocol) | ✅ | `grotto-chat-client/src/crypto/` |
| Channel messaging | ✅ | `grotto-chat-client/src/net/message_handler.cpp` |
| DM / private messages | ✅ | `grotto-chat-client/src/net/message_handler.cpp` |
| Offline sync markers | ✅ | `grotto-chat-client/src/net/message_handler.cpp` |
| Presence / away / DND / whois details | ✅ | `grotto-chat-client/src/app.cpp` |
| File transfers + files panel | ✅ | `grotto-chat-client/src/file/` + `grotto-chat-client/src/ui/files_panel.cpp` |
| Mouse support | ✅ | `grotto-chat-client/src/ui/mouse_support.cpp` |
| Clickable channel tabs | ✅ | `grotto-chat-client/src/ui/tab_bar.cpp` |
| Link previews | ✅ | `grotto-chat-client/src/preview/link_previewer.cpp` |
| IRC-style commands | ✅ | `grotto-chat-client/src/input/command_parser.cpp` |
| Message search (`/search`) | ✅ | `grotto-chat-client/src/app.cpp` |
| Settings UI | ✅ | `grotto-chat-client/src/ui/settings_screen.cpp` |
| Themes | ✅ | `grotto-chat-client/src/ui/color_scheme.hpp` |
| Clipboard integration | ✅ | `grotto-chat-client/src/input/input_handler.cpp` |
| Text selection | ✅ | `grotto-chat-client/src/ui/message_view.cpp` |
| Tab completion | ✅ | `grotto-chat-client/src/input/tab_complete.cpp` |
| Certificate pinning | ✅ | `grotto-chat-client/src/net/net_client.cpp` |
| Client-side rate limiting | ✅ | `grotto-chat-client/src/net/net_client.cpp` |
| Help system | ✅ | `grotto-chat-client/src/help/help_manager.cpp` |
| Voice P2P calls | ✅ | `grotto-chat-client/src/voice/voice_engine.cpp` |
| Group voice | ✅ | `grotto-chat-client/src/voice/voice_engine.cpp` |

Voice nykytilassa:
- `/call <user>` toimii
- `/vmode` tukee PTT / VOX
- mute / deafen toimii
- speaking indicators toimii
- ICE/TURN on konfiguroitavissa
- WebRTC noise suppression + limiter + jitter buffer on mukana

---

## Post-Release / Nice to Have

### UI / UX
- [ ] UI scaling (`Ctrl` + `+/-`)
- [ ] `/window` tai vastaava ikkunoiden hallinta
- [ ] Notification UX:n tarkempi hienosaato

### Voice
- [ ] Audio echo test ennen puhelua
- [ ] Per-user volume control
- [ ] Call quality indicators (latency / packet loss)
- [ ] Voice recording (opt-in)

### Integrations
- [ ] Native OS notifications
- [ ] URL scheme handler (`ircord://`)
- [ ] Auto-updater

---

## Known Open Risks

Katso tarkempi bugilista: [grotto-chat-client/BUGS.md](./grotto-chat-client/BUGS.md)

Avoimet release-riskit:
- Long-running session stability
- Platform build matrixa ei ole viela ajettu loppuun

---

## Current Priorities

1. Error-scenario QA ja platform build testit
2. Crypto state recovery / session repair polish
3. Release packaging + troubleshooting docs
4. UI scaling / post-release UX-poiminnat

---

*Paivita dokumentti kun blocker-status muuttuu.*
