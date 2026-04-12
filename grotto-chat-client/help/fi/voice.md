# Ääni

## Soittaminen

`/call <nimi>`       soita käyttäjälle yksityispuhelu  
`/accept`            vastaa, kun joku soittaa sinulle  
`/voicetest`         käynnistä tai pysäytä paikallinen mikrofonin loopback-testi  

## Puhelun aikana

`/mute`              mykistä oma mikrofoni  
`/deafen`            mykistä kaikki tulevat äänet  
`/vmode`             kierrä Toggle to Talk-, PTT- ja VOX-tilojen välillä  
`/hangup`            lopeta puhelu  

`Toggle to Talk` -tilassa paina valittua puhenäppäintä aloittaaksesi tai lopettaaksesi lähetyksen. `PTT`-tilassa pidä puhenäppäintä pohjassa puhuessasi. `VOX`-tilassa ääni lähtee automaattisesti kun puhut.

## Asetukset

Avaa `/settings` ja valitse oikeat äänilaiteet. Jos et kuule mitään, tarkista että oikea toistolaite on valittu.

`/voicetest` käyttää samaa paikallista capture- ja playback-polkuja kuin puhelut, mutta ei luo WebRTC-istuntoa. `Toggle to Talk` -tilassa paina puhenäppäintä kerran aloittaaksesi tai lopettaaksesi kuuntelun. `PTT`-tilassa pidä puhenäppäintä pohjassa kuunnellessasi. `VOX`-tilassa puhu normaalisti ja client toistaa mikrofonin valittuun ulostulolaitteeseen.

---

Huom: Ääni kulkee suoraan peer-to-peer -yhteydellä WebRTC:n kautta. Serveri ei kuule puheluitasi.
