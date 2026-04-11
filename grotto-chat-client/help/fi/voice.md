# Ääni

## Soittaminen

`/call <nimi>`       soita käyttäjälle yksityispuhelu  
`/accept`            vastaa, kun joku soittaa sinulle  
`/voicetest`         käynnistä tai pysäytä paikallinen mikrofonin loopback-testi  

## Puhelun aikana

`/mute`              mykistä oma mikrofoni  
`/deafen`            mykistä kaikki tulevat äänet  
`/vmode`             vaihda paina-puhu- ja VOX-tilan v„lill„  
`/hangup`            lopeta puhelu  

PTT-tilassa paina valittua PTT-näppäintä (`F1` oletuksena) aloittaaksesi tai lopettaaksesi lähetyksen. VOX-tilassa ääni lähtee automaattisesti kun puhut.

## Asetukset

Avaa `/settings` ja valitse oikeat äänilaiteet. Jos et kuule mitään, tarkista että oikea toistolaite on valittu.

`/voicetest` käyttää samaa paikallista capture- ja playback-polkuja kuin puhelut, mutta ei luo WebRTC-istuntoa. `PTT`-tilassa kuuntelet mikrofonia painamalla valittua PTT-näppäintä. `VOX`-tilassa puhu normaalisti ja client toistaa mikrofonin valittuun ulostulolaitteeseen.

---

Huom: Ääni kulkee suoraan peer-to-peer -yhteydellä WebRTC:n kautta. Serveri ei kuule puheluitasi.
