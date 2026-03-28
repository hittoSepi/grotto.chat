# TODOS

    - [ ] servulle jää quitanneet nimet kummitelemaan (servun ongelma)
    - [ ] servulle yhteyden tekeminen "hidas", katso `dev_docs/desktop-client/becoming-online.png`
      - voisi ainakin ilmoittaa paremmin että vielä connectaa
    - [ ] ctrl+c sammuttaa vieläkin ohjelman.
    - [x] ei koko ruudun levyistä underscorea, vain tekstin kohdalle. näyttää nyt vähä hölmöltä
    - [ ] paste ei toimi ainakaan powershellillä ssh läpi clientille
    - [ ] hiiren click koordinaatit saattavat olla väärin
    - [ ] right click linkin avaus toimii aika randomisti, clientillä toimii ku right klikkaa vasempaan yläkulmaan ja ps ssh client ei ollenkaan.
    - [x] priva viestin vastaanottajalla aukeaa tabi omalla nimellä, eikä sen kuka lähettänyty
    - [x] priva viestin vastaanottajalla ei päivity userlist
    - [ ] parempi /help ja jotkin alkuohjeet miten toimia kun servulle päästy, peruskäyttäjä ei välttämättä tiedä että pitää /join #<kanava> ja niin edelleen
    - [ ] private messaget eivät toimi
        ```grotto.chat
            privatechat msg [decryption failed]

            [2026-03-29 00:07:34.174] [grotto] [warning] Decryption failed for message from hittoLinux: -1005
            [2026-03-29 00:07:42.542] [grotto] [warning] Decryption failed for message from hitto: -1005
            [2026-03-29 00:07:48.168] [grotto] [warning] Decryption failed for message from hittoLinux: -1005
            [2026-03-29 00:07:55.846] [grotto] [debug] KEY_REQUEST sent for 'hittoLinux', plaintext queued
            [2026-03-29 00:07:55.900] [grotto] [debug] Received KeyBundle payload: size=184
            [2026-03-29 00:07:55.900] [grotto] [debug] Parsed KeyBundle: recipient_for='hittoLinux'
            [2026-03-29 00:07:55.903] [grotto] [info] Established X3DH session with 'hittoLinux'
            [2026-03-29 00:07:55.903] [grotto] [debug] Flushed 1 pending DM(s) to 'hittoLinux' (0 failed)
        ```