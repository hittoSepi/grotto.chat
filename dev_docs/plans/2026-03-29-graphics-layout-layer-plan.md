# Graphics Layout Layer for Grotto Client

## Summary

Tavoite on siirtää `grotto-chat-client` pois mallista, jossa kaikki chat-alueen sisältö oletetaan tekstiriveiksi. Graphics layout layer tekee chat-alueesta lohkoperusteisen: kuvat ja myöhemmät graafiset elementit voivat näkyä suoraan viestivirrassa, osallistua scrolliin, resizeen ja hit-testingiin, ja käyttää samaa perusrakennetta kuin myöhemmät kortit, kaaviot tai muut visuaaliset blokit.

Nykyisen toteutuksen suurin ongelma on ollut se, että native terminal graphics (`sixel`, kitty) ja FTXUI-tekstilayout ovat eläneet eri maailmoissa. Tämä suunnitelma rajaa FTXUI:n edelleen tekstikromiin ja inputiin, mutta chat-alueen sisäinen layout päätetään erillisessä välikerroksessa.

## Key Changes

- Lisää uusi välikerros viestimallin ja FTXUI-renderin väliin:
  - `MessageLayoutEngine` rakentaa aktiivisen kanavan viesteistä `LayoutBlock`-listan.
  - `LayoutBlock` on chat-alueen todellinen render-yksikkö tekstirivin sijaan.
- Määrittele block-tyypit v1:
  - `TextBlock`
  - `ImageBlock`
  - `PreviewBlock`
  - `SpacerBlock`
- Erottele renderointi kahteen vaiheeseen:
  - `measure/layout pass`
  - `paint pass`
- Lisää graphics backend -rajapinta:
  - `GraphicsBackend::supports(...)`
  - `GraphicsBackend::measure(...)`
  - `GraphicsBackend::draw(...)`
  - v1-backendit: `ColorBlockBackend`, `SixelBackend`, `KittyBackend`
- Korvaa nykyinen rivipohjainen näkyvyys- ja hit-test-API block-pohjaisella näkyvyysmallilla.
- Pane jää talteen erillisenä toissijaisena surface-komponenttina eikä sitä poisteta; se ei kuitenkaan saa olla ainoa tapa nähdä grafiikkaa.

## Implementation Changes

- `src/state/channel_state.hpp`
  - lisää render-osia tukeva `MessageRenderPart`.
- `src/ui/graphics_layout.*`
  - sisältää block/render-backend-abstraktion ja inline-kuvien mittaus/renderöintilogiikan.
- `src/ui/message_view.*`
  - renderöi viestit blockeina eikä pelkkinä tekstiriveinä.
  - palauttaa näkyvät hit-targetit block-tasolla.
- `src/ui/ui_manager.*`
  - käyttää uusia layout-hittejä hiiriosumiin.
  - chat-alue ei enää saa nojata siihen oletukseen, että kaikki näkyvä sisältö on tekstirivejä.
- `src/preview/link_previewer.*` ja `src/app.cpp`
  - kuva-previewit syöttävät render-osat block-layerille.

## Test Plan

- Tekstiviestit renderöityvät kuten ennen.
- Kuvallinen preview näkyy suoraan chat-virrassa ilman panea.
- Scroll kuvan yli toimii ilman että linkkiosumat siirtyvät.
- Resizen jälkeen kuvan block-korkeus lasketaan uudelleen oikein.
- Fallback toimii myös terminaaleissa ilman native graphics -tukea.

## Assumptions

- V1 keskittyy chat-alueeseen.
- FTXUI säilyy pää-UI-kirjastona.
- Pane säilytetään jatkokäyttöön toisena näyttöpintana.
- `Message::inline_image` saa toimia siirtymävaiheen datalähteenä, mutta block-layer on jatkossa ensisijainen render-malli.
