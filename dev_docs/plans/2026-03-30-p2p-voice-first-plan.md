## P2P Voice First

### Summary
Tehdään ensin `PeerToPeer voice toimimaan` ja jätetään `GroupVoice` seuraavaksi vaiheeksi.

Peruste:
- 1:1-polku on selvästi rajatumpi ja nykyisessä koodissa jo pitkällä.
- Samat kriittiset perustukset tarvitaan myös group-voicelle: signaling, peer connection lifecycle, audio capture/playback, PTT/VOX gating ja UI-state.
- Koodista löytyi suora blocker: `MessageHandler::handle_voice_signal()` ei tällä hetkellä välitä signaaleja `VoiceEngine`lle lainkaan, joten koko voice-stack ei voi toimia kunnolla ennen tämän korjaamista.
- Kun 1:1 toimii end-to-end, group voice voidaan rakentaa saman toimivan peer-polun päälle paljon turvallisemmin.

### Key Changes
1. Signaling ketju kuntoon
- Otetaan `MessageHandler::handle_voice_signal()` oikeasti käyttöön niin, että kaikki `VoiceSignal`-viestit forwardoidaan `VoiceEngine::on_voice_signal(...)`-funktiolle.
- Varmistetaan, että `/call`, `/accept` ja `/hangup` käyttävät samaa toimivaa signaling-polkuja loppuun asti.
- Pidetään ensimmäinen milestone 1:1-calliin rajattuna: ei yritetä samalla korjata room-signalingia pidemmälle kuin on pakko.

2. VoiceEngine 1:1 lifecycle eheäksi
- Korjataan `VoiceEngine` niin, että 1:1-callin tila päivittyy johdonmukaisesti:
  - `in_voice`
  - `active_channel`
  - `participants`
  - `voice_mode`
  - mute/deafen/PTT state
- `call()` ja `accept_call()` asettavat UI-tilan niin, että status bar ja user list saavat yhden osallistujan 1:1-puheluun.
- `hangup()` ja remote hangup palauttavat tilan varmasti puhtaaksi ilman roikkuvia peer-yhteyksiä tai osallistujia.
- WebRTC state change -callbackeissa kytketään connected/disconnected/failed näkyvästi voice stateen eikä vain logiin.

3. Audio path toimivaksi oikeassa käytössä
- Varmistetaan, että capture -> Opus encode -> RTP send -> decode -> jitter buffer -> playback toimii 1:1-polussa aidosti.
- PTT/VOX-gating pidetään nykyisen mallin mukaisena:
  - `ptt`: lähetetään vain kun PTT on aktiivinen
  - `vox`: lähetetään vain kun energia ylittää kynnyksen
- Ensimmäisessä vaiheessa ei tehdä erillistä “echo test” -moodia osaksi tätä milestonea; se voidaan tehdä myöhemmin omana audio diagnostics -featureenä.
- Jos named device selection ei vielä oikeasti valitse laitteita miniaudiosta, se kirjataan rajaukseksi eikä sotketa tätä milestonea siihen. Ensimmäinen tavoite on toimiva default-device 1:1 voice.

4. UI/UX ja käyttäjäpalautteet
- Incoming call, call accepted, hangup, connection failed ja disconnect näkyvät käyttäjälle johdonmukaisina system/voice-event-viesteinä.
- 1:1-puhelun aikana status bar näyttää voice-moden ja osallistujan oikein.
- Speaking indicator päivitetään myös 1:1-puhelussa saman `refresh_speaking_state()`-polun kautta kuin room-voicessa.
- Ei avata group voice -UX:ää tässä vaiheessa pidemmälle kuin nykyinen toimiva minimi.

### Public Interfaces / Types
- `MessageHandler`:
  - `handle_voice_signal(const Envelope&)` muuttuu passiivisesta debug-stubista aktiiviseksi forwarderiksi `VoiceEngine`lle.
- `VoiceState`:
  - käytetään 1:1-puhelussa eksplisiittisesti myös `participants`-kenttää yhden peerin listana, jotta UI-polut ovat yhtenäiset.
- `VoiceEngine`:
  - 1:1 call lifecycle (`call`, `accept_call`, `hangup`, `on_voice_signal`) täsmennetään niin, että tilasiirtymät ovat yksiselitteiset ja UI-safe.

### Test Plan
1. Build
- `grotto-client` buildaa.
- Ei uusia compile/link regressioita voice-, ui- tai message-handler-poluille.

2. Manual runtime scenarios
- Käyttäjä A soittaa käyttäjälle B komennolla `/call <nick>`.
- Käyttäjä B saa incoming call -ilmoituksen ja hyväksyy `/accept <nick>`.
- SDP/ICE exchange menee läpi ja peer connection siirtyy `Connected`-tilaan.
- A puhuu, B kuulee äänen.
- B puhuu, A kuulee äänen.
- `ptt`-tilassa ääntä ei lähde ilman PTT-aktivointia.
- `vox`-tilassa ääntä lähtee vain kynnyksen ylittyessä.
- `/mute` estää lähetyksen.
- `/deafen` estää toiston.
- `/hangup` sulkee puhelun molemmissa päissä ja siivoaa UI-tilan.
- Remote disconnect / failed connection näyttää virheen eikä jätä voice-tilaa roikkumaan.

3. Regression checks
- Room voice -koodi edelleen buildaa eikä 1:1-fixit riko nykyistä `/voice` join/leave -polkua.
- Status bar ja user list eivät näytä ghost-participants 1:1-puhelun jälkeen.

### Assumptions
- Ensimmäinen voice-milestone on 1:1 P2P call, ei group voice.
- Named input/output device selection ei ole tämän vaiheen pakollinen acceptance criterion, jos default-device call muuten toimii.
- Echo/input-self-test jää erilliseksi myöhemmäksi audio diagnostics -tehtäväksi.
- Group voice rakennetaan tämän jälkeen saman korjatun signaling- ja peer-lifecycle-pohjan päälle.
