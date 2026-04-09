# Agent Guidelines

Tämä dokumentti sisältää ohjeet tekoälyagenteille Grotto-projektissa.

## Periaatteet

### 1. Siistiä koodia

- **Selkeä nimeäminen**: Muuttujien, funktioiden ja luokkien nimien tulee kuvata selkeästi niiden tarkoitusta.
- **Yksi vastuu**: Jokaisella funktiolla ja moduulilla tulee olla yksi selkeä vastuu.
- **Deduplikointi**: Älä kopioi koodia - refaktoroi yhteinen logiikka uudelleenkäytettäviksi funktioiksi.
- **Virheenkäsittely**: Käsittele virheet eksplisiittisesti äläkä jätä poikkeuksia sieppaamatta.
- **Tyypiturvallisuus**: Hyödynnä C++20:n vahvaa tyypitystä ja vältä `void*`-pointereita.
- **Slash-komennot**: Jos teet uusia slash-komentoja, tee siitä myös commands helpfileen merkintä.
### 2. Pienet diffit

- **Minimalistiset muutokset**: Tee vain ne muutokset jotka ovat välttämättömiä tehtävän suorittamiseen.
- **Yksi asia kerrallaan**: Älä sekoita useita ominaisuuksia tai korjauksia samaan committiin.
- **Vältä turhia muutoksia**: Älä formatoi koodia tai poista whitespacea ellei se liity tehtävään.
- **Koodin tyyli**: Noudata projektin olemassa olevaa koodityyliä (4 välilyöntiä, snake_case C++:ssa, PascalCase luokat).

### 3. Dokumentoi tehtävän jälkeen

- **README-päivitykset**: Jos lisäät uusia komentoja tai muutat käyttöliittymää, päivitä relevantit README-tiedostot.
- **CHANGELOG-merkinnät**: Merkitse merkittävät muutokset CHANGELOG.md-tiedostoon.
- **AGENTS.md**: Jos muutat työskentelytapoja tai lisäät uusia konventioita, päivitä tämä tiedosto.
- **Koodikommentit**: Dokumentoi monimutkaiset algoritmit ja kryptografiset operaatiot.
- **Muisti**: Päivitä tarvittaessa agenttimuistiin `memory/<yyyy-MM-dd>.md`, jotta tulevat agentit voivat hyödyntää opittua.

## Projektirakenne

```
grotto-chat/
├── grotto-server/       # C++20 - Palvelin (TLS/TCP relay)
├── grotto-client/       # C++20 - Desktop TUI-asiakas
├── grotto-android/      # Kotlin/C++ - Android-asiakas
├── grotto-server-tui/   # C++20 - Admin TUI
├── grotto-server-api/   # C++20 - HTTP API
├── grotto-plugin/       # C++/QuickJS - Plugin-järjestelmä
├── grotto-infra/        # Docker/Node.js - Infrastruktuuri
├── grotto-installer/    # C++20 - Linux-asennusvelho
└── grotto-daemon/       # Taustaprosessi
```

## Teknologiapino

| Komponentti | Teknologia |
|-------------|------------|
| Kieli | C++20 / Kotlin |
| Async I/O | Boost.Asio |
| Serialisointi | Protobuf |
| E2E-salaus | libsignal-protocol-c + libsodium |
| Tietokanta | SQLite + SQLCipher |
| UI (Desktop) | FTXUI |
| UI (Android) | Jetpack Compose |
| Ääni | WebRTC + Opus |

## Tärkeät tiedostot

- `README.md` - Yleiskatsaus ja pika-aloitus
- `CHANGELOG.md` - Muutoshistoria
- `TODO.md` - Kehityslista
- `UX-PLAN.md` - UX-suunnitelma
- `ICE-PLAN.md` - ICE/VoIP-suunnitelma

## Build-ohjeet

### C++-komponentit
```bash
cd <komponentti>
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Android
```bash
cd grotto-android
./gradlew assembleDebug
```

## Muistutus

> Mikäli olet epävarma jostakin, kysy käyttäjältä selvennystä. On parempi kysyä kuin tehdä oletuksia.
