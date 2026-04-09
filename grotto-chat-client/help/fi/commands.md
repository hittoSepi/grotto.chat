# Komennot

## Perusjutut

`/msg <nimi> <teksti>`     lähetä yksityisviesti (alias: `/w`)  
`/me <toiminta>`           lähetä toimintaviesti (* Rausku juo kahvia)  
`/nick <uusi_nimi>`        vaihda nimimerkkiä  

## Kanavat

`/join <#kanava>`          liity kanavalle (nimi alkaa #, alias: `/j`)  
`/part [#kanava]`          poistu nykyiseltä tai nimetyltä kanavalta (alias: `/p`)  
`/names`                   listaa paikalla olevat (alias: `/ns`)  
`/whois <nimi>`            näytä käyttäjän tiedot  
`/away [syy]`              merkitse itsesi poissa-tilaan  
`/afk [syy]`               alias komennolle `/away`  
`/back`                    palaa takaisin online-tilaan  
`/dnd [syy]`               merkitse itsesi älä häiritse -tilaan  

## Ääni

`/call <nimi>`             soita käyttäjälle  
`/accept`                  vastaa saapuvaan puheluun  
`/hangup`                  lopeta puhelu  
`/voice`                   kytke äänikanava päälle/pois  
`/mute`                    mykistä mikrofoni  
`/deafen`                  mykistä kaikki äänet  
`/vmode`                   vaihda `PTT`- ja `VOX`-tilan v„lill„  

## Turva

`/trust <nimi>`            merkitse käyttäjän avain luotetuksi  

## Työkalut

`/search <haku>`           etsi viestihistoriasta (tukee FTS5-syntaksia)  
  Esimerkkejä:  
    /search moi              etsi "moi" nykyiseltä kanavalta  
    /search moi maailma      etsi viestejä jotka sisältävät molemmat sanat  
    /search "moi maailma"    etsi tarkkaa fraasia  
    /search user:Sepi        etsi käyttäjän Sepi viestejä  
    /search day:2026-04-09   etsi tietyn päivämäärän viestejä  
    /search moi user:Sepi    yhdistetty haku (sana + käyttäjäfiltteri)  
`/clear`                   tyhjennä viestinäkymä  
`/settings`                avaa asetusnäyttö  
`/version`                 näytä clientin ja serverin versio (alias: `/ver`)  
`/status`                  näytä yhteystila (alias: `/st`)  
`/diag ui`                 näytä UI/leikepöytä/grafiikka -diagnostiikka  
`/help [aihe]`             näytä tämä ohje tai tietty aihe (alias: `/h`)  
`/reload_help`             lataa ohjetiedostot uudelleen levyltä (alias: `/rh`)  

## Yhteys

`/disconnect`              katkaise yhteys serveriin  
`/quit`                    sulje Grotto lopullisesti (alias: `/q`)  

## Tiedostot

`/upload <polku>`          lähetä tiedosto (alias: `/up`)  
`/download <id> [polku]`   lataa tiedosto (alias: `/dl`)  
`/transfers [määrä]`       näytä viimeisimmät tiedostosiirrot (alias: `/xfers`)  
`/files`                   päivitä aktiivisen kanavan/DM:n tiedostot ja avaa files-paneli (alias: `/ls`)  
`/downloads`               avaa paikallinen downloads-kansio (alias: `/dir`)  
`/quota`                   näytä nykyinen tiedostotilan käyttö, rajat ja jäljellä oleva tila
`/rmfile <id>`             poista oma lähettämäsi tiedosto (tai kanavatiedosto jos olet operaattori)  

---

Vihje: Komento täydentyy `Tab`-näppäimellä. Kirjoita `/` ja paina Tab.
`F3` files-panelissa `Ctrl+F` avaa haun, `s` vaihtaa lajittelua, `r` päivittää listan, `Del` poistaa ja `o` avaa downloads-kansion.
