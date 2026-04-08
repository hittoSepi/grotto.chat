# KNOWN BUGS & ISSUES & TODOs

  - [x] Asetus sivulla tabeista ei pääse itse asetuksiin, hiirellä toimii.
  - [x] DM kanavalle ilmestyi kolmas user jostain syystä.
  - [x] Offline viesti toimii mutta userlista ei näytä omaa nimeä lähettäjän vain.
  - [x] Kuva Preview ei näy muille vastaanottajille, mutta itselle näkyy.
  - [x] Jollain Terminaalilla tulee kuvapreview pillossa olevilla "#" merkeillä, mutta ne voi maalata ja kopioida hiirellä.
      ```log
      [23:06] [image] https://grotto.chat/images/grotto-daemon-avatar.png
        image/png
        ########################################
        ########################################
        ########################################
        ########################################
        ########################################
        ########################################
        ########################################
        ########################################
      ```
  
  - [x] Muista kanavat mihin liittynyt, ja DM:t, jotta ei tarvitse joka kerta hakea uudestaan.
  - [x] Windows-clientillä DM voi jäädä tilaan jossa vastaanotettu viesti näkyy `[decryption failed]`. Paikallisten tiedostojen (`grotto.db`, `identity.key`, `credentials.enc`) poisto auttoi, eli ongelma näyttää liittyvän vanhaan local crypto/session-stateen.
