# Safe Main-Thread Native Graphics Compositor

## Summary
- Tavoite: saada native kuvat näkymään suoraan chat-virrassa ilman kaatumisia.
- Nykyinen block-pohjainen graphics layout on jo olemassa, mutta ensimmäinen native inline -piirtopolku osoittautui epävakaaksi, koska se käytti `WithRestoredIO`-kutsuja taustasäikeestä renderin jälkeen.
- Valittu suunta: erotetaan oma `GraphicsCompositor`, joka elää UI:n pääsäikeen render-elinkaaren sisällä ja saa layout-engineltä valmiit `GraphicsDrawCommand`-komennot.

## Architecture
- `MessageLayoutEngine` / `message_view.cpp`
  - tuottaa FTXUI-rivit
  - tuottaa näkyvät `GraphicsDrawCommand`-komennot samasta viewportista
- `GraphicsCompositor`
  - omistaa viimeisimmän framen, clear/draw-päätökset ja backend-commitin
  - tietää terminal-native piirron backendit, mutta ei tee viestilayoutia
- `UIManager`
  - rakentaa documentin
  - välittää compositorille frame-datan
  - ei enää spawn-aa render-säikeitä tai tee ad hoc -draw-kokeiluja

## Frame Model
- `GraphicsFrame`
  - `viewport_x`
  - `viewport_y`
  - `viewport_width`
  - `viewport_height`
  - `commands`
- `GraphicsCompositorState`
  - `last_frame`
  - `had_native_graphics`
  - `requires_full_clear`

## Commit Rules
- Jos native inline graphics ei ole erikseen aktivoitu, compositor tekee vain bookkeepingin eikä koske terminaliin.
- Jos frame ei sisällä native-komentoja eikä aiemmin ollut native-grafiikkaa, ei tehdä mitään.
- Jos resize, scroll tai kanavanvaihto invalidioi framen, compositor merkitsee `requires_full_clear = true`.
- Varsinainen native backend -commit lisätään vasta kun pääsäiepiirron turvallinen toteutus on valmis.

## Backend Strategy
- Kitty
  - pidä placement-ID:t hallinnassa
  - tue explicit clear vanhoille placementeille
- Sixel
  - käytä aluksi full-redraw-strategiaa viewportille
  - vältä osittaista clear-logiikkaa ensimmäisessä turvallisessa versiossa
- Color-block fallback
  - pysyy aina käytettävissä
  - compositor ei omista fallback-renderöintiä

## Implementation Phases
1. Erottele `GraphicsCompositor` omiin tiedostoihin ja siirrä nykyinen draw-state sinne.
2. Korvaa `UIManager`in nykyinen taustasäieviritys compositor-kutsulla render-loopissa.
3. Lisää frame diff / invalidointi / clear-päätökset ilman native draw -sivuvaikutuksia.
4. Lisää turvallinen pääsäie-commit kittylle.
5. Lisää turvallinen pääsäie-commit sixelille.

## Verification
- Client käynnistyy ja yhdistää normaalisti ilman kuvia.
- Kuvapreview ei kaada clientiä.
- Fallback-preview näkyy edelleen.
- Compositor kerää näkyvät native draw -komennot ilman että niitä vielä piirretään oletuksena.
