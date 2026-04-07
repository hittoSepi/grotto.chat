# Siirrot

`/transfers [määrä]` näyttää viimeisimmät tiedostojen uploadit ja downloadit.

Esimerkkejä:
```
/transfers
/transfers 5
```

Jokainen rivi näyttää:
- suunnan (`upload` tai `download`)
- paikallisen transfer-id:n
- tilan (`queued`, `uploading`, `downloading`, `completed`, `failed`, `cancelled`)
- etenemisen prosentteina ja tavuina
- kohdekanavan / DM:n tai paikallisen tallennuspolun

Aktiiviset siirrot näkyvät myös status barissa.
