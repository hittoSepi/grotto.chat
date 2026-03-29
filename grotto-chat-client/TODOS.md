# TODOS

    - [ ] servulle jää quitanneet nimet kummitelemaan (servun ongelma)
    - [ ] servulle yhteyden tekeminen "hidas", katso `dev_docs/desktop-client/becoming-online.png`
      - voisi ainakin ilmoittaa paremmin että vielä connectaa
    - [ ] ctrl+c sammuttaa vieläkin ohjelman.
    - [x] jos kirjoitat /help se ei mahdu koko ruutuun mutta rullausalue ei muutu
    - [x] ei koko ruudun levyistä underscorea, vain tekstin kohdalle. näyttää nyt vähä hölmöltä
    - [ ] paste ei toimi ainakaan powershellillä ssh läpi clientille
    - [x] tekstin valinta chat-ruutuun
    - [ ] hiiren click koordinaatit saattavat olla väärin
    - [ ] right click linkin avaus toimii aika randomisti, clientillä toimii ku right klikkaa vasempaan yläkulmaan ja ps ssh client ei ollenkaan.
    - [x] priva viestin vastaanottajalla aukeaa tabi omalla nimellä, eikä sen kuka lähettänyty
    - [x] priva viestin vastaanottajalla ei päivity userlist
    - [x] jos msg vastaanottajaa ei ole, anna palautetta userille
    - [x] parempi /help ja jotkin alkuohjeet miten toimia kun servulle päästy, peruskäyttäjä ei välttämättä tiedä että pitää /join #<kanava> ja niin edelleen
    - [x] käyttäjän viestin pituudessa pitää olla raja
      - client plaintext max 4096 tavua
      - server encrypted chat payload configurable `max_chat_payload_bytes`
    - [x] private messaget eivät toimi, viestit "tulevat" perille muttaa decryptaus failaa
        ```grotto.chat
            privatechat msg [decryption failed]
            [grotto] [warning] Decryption failed for message from hittoLinuxi: -1005

            [grotto] [debug] KEY_REQUEST sent for 'hittoLinux', plaintext queued
            [grotto] [debug] Received KeyBundle payload: size=184
            [grotto] [debug] Parsed KeyBundle: recipient_for='hittoLinux'
            [grotto] [info] Established X3DH session with 'hittoLinux'
            [grotto] [debug] Flushed 1 pending DM(s) to 'hittoLinux' (0 failed)
            [grotto] [warning] Decryption failed for message from hittoLinux: -1005
        ```
