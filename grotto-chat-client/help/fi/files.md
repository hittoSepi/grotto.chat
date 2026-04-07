# Tiedostot

## Lähettäminen

`/upload <polku>` lähettää tiedoston aktiiviselle kanavalle tai käyttäjälle.
`/transfers [määrä]` näyttää aktiiviset ja viimeisimmät siirrot.

Esimerkki:
```
/upload C:\kuvat\meme.png
/upload ~/dokumentit/salaisuudet.txt
```

## Vastaanottaminen

Kun joku lähettää tiedoston, Grotto näyttää ilmoituksen. Käytä `/download` tallentaaksesi tiedoston.

## Turva

Tiedostot on salattu samalla tavalla kuin viestit. Serveri vain kuljettaa palasia eteenpäin ilman että näkee sisältöä.

Jos serveri ilmoittaa tiedostopolitiikan, client estää liian isot tai kiellettyä MIME-tyyppiä käyttävät uploadit jo ennen lähetystä.
