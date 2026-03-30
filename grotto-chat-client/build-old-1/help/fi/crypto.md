# Salaus & Luottamus

Grottossa serveri kuljettaa viestit, mutta ei ikinä näe niiden sisältöä. Kaikki on salattu päästä päähän.

## Miten se toimii

- Jokainen käyttäjä saa oman `Ed25519`-tunnisteavaimen ensimmäisellä käynnistyksellä
- Yksityisviestit käyttävät **Signal Protocolia** (X3DH + Double Ratchet)
- Kanavaviestit käyttävät **Sender Key** -salausta tehokkaaseen ryhmäsalaamiseen

## Luottaminen

Kun joku lähettää viestejä, Grotto näyttää heidän julkisen avaimensa. Ensimmäisen kerran se on "tuntematon".

Kirjoita `/trust <nimi>` varmistaaksesi, että puhut oikealle ihmiselle. Jos avain muuttuu myöhemmin, Grotto varoittaa sinua.

## Avainten hallinta

Tunnisteavaimesi on tallennettu paikallisesti ja suojattu salasanallasi. Jos unohdat salasanasi tai haluat aloittaa puhtaalta pöydältä:

1. Sulje Grotto
2. Käynnistä uudelleen `--clear-creds`-lipulla
3. Kirjaudu samalla nimimerkillä ja uudella salasanalla
4. Serverin avaimet synkronoituvat uudelleen
5. Muista luottaa kontakteihisi uudelleen

---

Serveri näkee: kuka puhuu kenelle, milloin, ja mistä IP:stä.  
Serveri EI NÄE: viestien sisällön, tiedostojen sisällön, äänipuheluiden ääntä.
