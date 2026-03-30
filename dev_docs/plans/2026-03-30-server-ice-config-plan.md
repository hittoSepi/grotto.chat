## Server-Provided ICE Config (Post-Auth)

### Summary
Lisätään serveriltä authin jälkeen clientille automaattisesti jaettava ICE/TURN-konfiguraatio, jotta käyttäjän ei tarvitse syöttää `[voice].ice_servers`, `turn_username` tai `turn_password` arvoja käsin `client.toml`:iin.

Ensimmäinen versio on tarkoituksella yksinkertainen:
- serveri lähettää staattisen ICE/TURN-configin authin jälkeen
- client tallentaa sen runtimeen ja käyttää sitä `VoiceEngine`ssä
- nykyinen `client.toml`-tuki jää fallbackiksi / overrideksi

Tämä ratkaisee nykyisen UX-ongelman: käyttäjä tietää vain chat-serverin osoitteen, mutta ei voi päätellä TURN-hostia, portteja tai credentiaaleja itse.

### Goals
- Poistaa manuaalinen TURN-asetusten kopiointi tavalliselta käyttäjältä.
- Mahdollistaa voice-yhteydet heti authin jälkeen serverin omilla tiedoilla.
- Säilyttää nykyinen config-malli fallbackina kehitystä ja paikallista overridea varten.

### Non-Goals
- Ei vielä lyhytikäisiä TURN-credentiaaleja.
- Ei vielä erillistä Settings-UI:ta serveriltä saadun ICE-configin näyttämiseen tai muokkaamiseen.
- Ei tässä vaiheessa auto-savea takaisin `client.toml`:iin.

### Design

#### 1. Uusi protoviesti serveriltä clientille
Lisätään protobufiin uusi viesti, esimerkiksi:

```proto
message IceServerConfig {
  string url = 1;
}

message VoiceIceConfig {
  repeated string ice_servers = 1;
  string turn_username = 2;
  string turn_password = 3;
}
```

Ja uusi message type, esimerkiksi:

```proto
MT_VOICE_ICE_CONFIG = <next free id>
```

Serveri lähettää tämän viestin authin jälkeen vain juuri autentikoidulle clientille.

#### 2. Serverin config-lähde
Serverille lisätään konfiguroitava ICE/TURN-lähde, esimerkiksi:
- `voice.ice_servers[]`
- `voice.turn_username`
- `voice.turn_password`

Ensimmäisessä versiossa nämä voivat tulla:
- serverin TOML/INI-configista, tai
- installerin generoimasta configista

Perusajatus:
- installer tietää jo TURN-domainin ja credentiaalit
- serveri lukee ne runtime-configista
- authin jälkeen serveri välittää ne clientille

#### 3. Lähetys authin jälkeen
Nykyiseen auth-success-polkuun lisätään:
1. `AUTH_OK`
2. mahdollinen muu bootstrap
3. `VOICE_ICE_CONFIG`

Näin clientillä on ICE/TURN-tiedot valmiina ennen kuin käyttäjä yrittää `/voice` tai `/call`.

#### 4. Client runtime override -malli
Clientin käyttäytyminen:
- jos serveriltä tulee `VOICE_ICE_CONFIG`, sitä käytetään aktiivisena runtime-configina voicelle
- jos server ei lähetä mitään, käytetään nykyistä `client.toml`-configia
- jos käyttäjä on määrittänyt paikallisen configin, voidaan aluksi päättää että serveriltä tullut tieto voittaa vain silloin kun paikallinen lista on tyhjä

Suositeltu ensimmäinen policy:
- `runtime_voice_ice_config` voittaa aina, jos serveriltä saadaan validi viesti
- muuten fallback `cfg.voice.*`

Tämä on käyttäjälle selkein, koska “serverin kanssa toimivat asetukset” ovat silloin aina käytössä.

#### 5. VoiceEngine-integraatio
`VoiceEngine::make_rtc_config()` päivitetään käyttämään ensisijaisesti runtime-ICE-configia, ei vain käynnistyshetken `cfg_`-rakennetta.

Tarvitaan joku näistä:
- `AppState`iin runtime `VoiceIceConfig`
- tai `VoiceEngine`lle setter, esim. `set_runtime_ice_config(...)`

Suositus:
- lisätään `AppState`iin kevyt runtime bootstrap/config -tila, jotta myös UI voi myöhemmin näyttää mistä voice-config tuli
- `VoiceEngine` lukee sen aina `make_rtc_config()`-kutsussa

#### 6. Installer-future fit
Nykyinen `install-ice.sh` ja myöhempi server installer voivat kirjoittaa samat TURN-tiedot myös serverin omaan configiin.

Tavoitetila:
- ylläpitäjä asentaa TURNin kerran
- serveri tietää ICE/TURN-asetukset
- clientit saavat ne automaattisesti authin jälkeen

### Key Changes
1. Protokolla
- Lisää `VoiceIceConfig` protobufiin
- Lisää `MT_VOICE_ICE_CONFIG`

2. Server
- Lisää runtime configiin ICE/TURN-kentät
- Lähetä `VOICE_ICE_CONFIG` auth-success-polussa

3. Client MessageHandler
- Lisää `handle_voice_ice_config(const Envelope&)`
- Parse + validate + tallenna runtimeen

4. Client state/runtime
- Lisää runtime-voice-bootstrap/ICE-config -tila
- `VoiceEngine::make_rtc_config()` käyttää runtime-arvoja, jos saatavilla

5. Mahdollinen UI myöhemmin
- Näytetään debug/status-näkymässä “voice bootstrap source: server/config”
- Ei pakollinen ensimmäisessä vaiheessa

### Test Plan
1. Build
- `grotto-server` buildaa protobuf-muutosten jälkeen
- `grotto-client` buildaa protobuf- ja handler-muutosten jälkeen

2. Manual
- Käynnistä serveri, jossa on TURN/ICE config määritelty
- Client autentikoituu ilman `[voice].ice_servers` kenttiä paikallisessa configissa
- Authin jälkeen client saa serveriltä ICE/TURN-tiedot
- `/voice` tai `/call` käyttää serveriltä saatua configia
- Voice toimii ilman että käyttäjä muokkaa `client.toml`:ia käsin

3. Fallback
- Jos server ei lähetä `VOICE_ICE_CONFIG`, client käyttää paikallista `client.toml`-configia kuten ennen
- Jos server lähettää tyhjän/virheellisen viestin, client loggaa virheen ja fallbackkaa paikalliseen configiin

### Risks
- Proto-muutos koskee sekä clienttiä että serveriä samaan aikaan
- Jos precedence-policy on epäselvä, käyttäjä voi ihmetellä miksi paikallinen config ei vaikuta
- Staattisen TURN-salasanan lähetys authin jälkeen on ok ensimmäinen vaihe, mutta ei ideaalinen pitkän aikavälin security-malli

### Follow-up
Seuraava vaihe tämän jälkeen:
- vaihdetaan staattinen `turn_password` lyhytikäiseen session credentialiin
- mahdollisesti lisätään TTL/expiry kenttä
- mahdollisesti UI/debug-näkymä aktiiviselle voice bootstrapille
