# Markdown-renderöinti + Help-järjestelmä

## Yhteenveto

Lisätään FTXUI-pohjaiseen desktop-clienttiin:
1. Markdown-renderöijä joka tukee keskitason markdown-elementtejä
2. Help-järjestelmä joka lukee .md-tiedostoja levyltä
3. Markdown-renderöinti myös chat-viesteihin

## 1. Markdown-renderöijä

**Tiedostot**: `src/ui/markdown_renderer.hpp`, `src/ui/markdown_renderer.cpp`

Funktio: `ftxui::Element render_markdown(const std::string& text)`

Tuetut elementit:
- `**bold**` → `ftxui::bold`
- `*italic*` → `ftxui::dim` (italic ei toimi kaikissa terminaaleissa)
- `` `inline code` `` → `ftxui::inverted`
- ` ```code block``` ` → `vbox` invertoiduilla riveillä
- `# otsikko` → `bold | underlined`
- `- lista` → sisennys + `• ` prefix
- `> lainaus` → `│ ` prefix + `dim`

Parseri käsittelee tekstin rivi kerrallaan:
1. Tunnista blokkitason elementit (otsikot, listat, lainaukset, koodiblokit)
2. Inline-elementit (bold, italic, code) parsitaan kunkin rivin sisällä

## 2. Help-manager

**Tiedostot**: `src/help/help_manager.hpp`, `src/help/help_manager.cpp`

```cpp
class HelpManager {
    std::map<std::string, std::string> cache_;
    std::filesystem::path help_dir_;
public:
    explicit HelpManager(const std::filesystem::path& binary_dir);
    void load();
    void reload();
    std::vector<std::string> topics() const;
    std::optional<std::string> get(const std::string& topic) const;
};
```

- Polku: binäärin sijainti + `/help/`
- `load()` — skannaa kansion, lukee kaikki .md-tiedostot cacheen
- `reload()` — tyhjentää cachen ja lataa uudelleen
- `get("help")` — palauttaa pääsivun (`help.md`)
- `get("commands")` — palauttaa `commands.md` jne.

## 3. Help-tiedostot

Sijainti: `help/` binäärin vieressä.

Tiedostot:
- `help.md` — pääsivu, custom tervetuloteksti (näytetään `/help` ilman args)
- `commands.md` — kaikki komennot ja käyttö
- `shortcuts.md` — pikanäppäimet
- `channels.md` — kanavien käyttö
- `voice.md` — puheominaisuudet
- `crypto.md` — salaus, trust, avaimet
- `files.md` — tiedostojen siirto

## 4. Komennot

- `/help` → renderöi ja näyttää `help/help.md`
- `/help <topic>` → renderöi ja näyttää `help/<topic>.md`, tai "Topic not found"
- `/reload_help` → kutsuu `help_manager_.reload()`, näyttää vahvistusviestin

Lisätään `/reload_help` tunnettuihin komentoihin `command_parser.cpp`:ssä.

## 5. Chat-viestien markdown

`message_view.cpp:render_one()` käyttää `render_markdown()` `paragraph()`:n sijaan viestisisällölle. Tämä mahdollistaa käyttäjien lähettämän markdown-sisällön renderöinnin.

## Turvallisuus

- Help-tiedostot luetaan vain paikalliselta levyltä — ei verkkosyötettä
- Chat-markdown on visuaalista — ei suoritettavaa koodia
- Tiedostopolku sanitoidaan (ei `../` -traversalia help-topicissa)
