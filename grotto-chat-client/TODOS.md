# TODOS

    - [ ] servulle jää quitanneet nimet kummitelemaan (servun ongelma)
    - [ ] servulle yhteyden tekeminen "hidas", katso `dev_docs/desktop-client/becoming-online.png`
      - voisi ainakin ilmoittaa paremmin että vielä connectaa
    - [ ] ctrl+c sammuttaa vieläkin ohjelman, windows console ongelma? (sammuttaa linuxullakkin ohjelman)
    - [x] ei koko ruudun levyistä underscorea, vain tekstin kohdalle. näyttää nyt vähä hölmöltä
    - [ ] paste ei toimi ainakaan powershellillä ssh läpi clientille
    - [ ] right click linkin avaus toimii aika randomisti, clientillä toimii ku right klikkaa vasempaan yläkulmaan ja ps ssh client ei ollenkaan
    - [ ] parempi /help ja jotkin alkuohjeet miten toimia kun servulle päästy, peruskäyttäjä ei välttämättä tiedä että pitää /join #<kanava> ja niin edelleen
    - [ ] private messaget eivät toimi
        ```log
        [2026-03-28 19:14:14.873] [grotto] [debug] KEY_REQUEST sent for 'hittoaeae', plaintext queued
        [2026-03-28 19:14:14.875] [grotto] [debug] Received KeyBundle payload: size=179
        [2026-03-28 19:14:14.875] [grotto] [debug] Parsed KeyBundle: recipient_for='hittoaeae'
        [2026-03-28 19:14:14.875] [grotto] [error] session_builder_process_pre_key_bundle failed for hittoaeae: -22
        [2026-03-28 19:14:24.873] [grotto] [warning] KEY_BUNDLE timeout for 'hittoaeae', 1 messages dropped
        ```