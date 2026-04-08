# KNOWN BUGS

  - [x] Asetus sivulla tabeista ei pääse itse asetuksiin, hiirellä toimii.
  - [x] DM kanavalle ilmestyi kolmas user jostain syystä.
  - [x] Offline viesti toimii mutta userlista ei näytä omaa nimeä lähettäjän vain.
  - [ ] Kuva Preview ei näy muille vastaanottajille, mutta itselle näkyy.
  - [ ] Windows-clientillä DM voi jäädä tilaan jossa vastaanotettu viesti näkyy `[decryption failed]`. Paikallisten tiedostojen (`grotto.db`, `identity.key`, `credentials.enc`) poisto auttoi, eli ongelma näyttää liittyvän vanhaan local crypto/session-stateen.
