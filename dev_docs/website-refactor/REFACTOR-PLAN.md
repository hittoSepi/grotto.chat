# **GROTTO.chat** – Installer Project

[Project folder: `./grotto-chat/landing`](./grotto-chat/landing)


      > *End-to-end encrypted chat and voice application for > friend groups.
      > Combines irssi-style minimal terminal aesthetics with modern features:*

- Signal Protocol E2E encryption
- voice rooms
- file transfers
- link previews
- lightweight native clients

<div class="margin-right:20px"><extremely low resource usage and self-hostable alternative to Discord-style platforms.*

---

**Example observation:**<br/>
- IRCord client ≈ few MB RAM<br/>
- Discord ≈ hundreds of MB**

### **Target audience:**

> - small friend groups
> - self-hosterd
> - people leaving Discord-style ecosystems
> - users who like IRC-style minimal tools
> - Current situation
> - Project is in one repo, installer handlers downloads.


### Typical structure includes things like:<br/>
**GROTTO**
grotto-client
grotto-server
grotto-server-api
grotto-server-tui
grotto-android
grotto-infra*

**The project already includes:**<br/>
  - C++ components
  - Terminal UI components
  - FTXUI usage elsewhere in the project
  - Android client
infrastructure repo
documentation site
Problem to solve
Installation is currently too complex for end users.
Typical developer install might involve:

git clone
cmake
build
configure
start services

---
### **Most users will not attempt this.<br/><small>(i'm mad)**</small>
---

**Goal:**
one-command installation experience.
Planned solution
Create a two-stage installer system.
Stage 1 – bootstrap
Very small script.

Linux example:
  curl -fsSL https://chat.rausku.com/install.sh | bash

Windows example:

irm https://chat.rausku.com/install.ps1 | iex

Responsibilities:

detect OS

detect architecture

download correct installer binary

launch installer

Nothing else.

Stage 2 – binary installer

Binary application called:

grotto-installer

Built with FTXUI.

Reason:

already used in project

no extra UI dependency

good for wizard style UI

Installer UX goal

Terminal wizard.


##M ikä tässä osuu


Useampi silmäpari taustalla
Tuo on erinomainen lisä. Heti tulee olo, että daemon ei ole vain yksi otus vaan ehkä koko luolan verkosto tai parvi. Se sopii täydellisesti salattuihin tunneleihin ja taustalla pyörivään järjestelmään.

Tummempi keskusta
Nyt katse menee kunnolla noihin hehkuviin silmiin. Aiempi versio oli vähän “hahmo poseeraa”. Tämä on enemmän “hahmo vartioi ja tarkkailee”. Selvästi parempi.

*'Vihreä hehku kristallissa** Toimii edelleen todella hyvin salausavaimen symbolina. Tuo on melkein valmis brändielementti jo.

Luolan syvyys
Photoshoppaus teki tästä enemmän paikan kuin kuvituksen. Nyt tämä näyttää siltä että luolassa on oikeasti käytäviä, ei vain yksi kivinen kaari.

Mikä tästä tulee mieleen brändin kannalta

Tämä voisi toimia jopa kolmella tasolla:

### 1. Installer / splash art

Tähän se sopii heti. Todella hyvin.

### 2. Landing page hero tausta

Varsinkin tummalla overlayllä ja tekstillä päälle:

Encrypted tunnels for friend groups

Private chat, voice and file transfer inside the cave

### 3. Lore / daemon identity

Taustasilmät tekevät tästä kiinnostavamman kuin pelkkä yksi maskotti. Nyt “grotto daemon” voi tarkoittaa:
yhtä vartijaa
useita daemon-olioita
koko luolan tietoisuutta
Aika hyvä vahinko, kuten nämä usein.
Yksi pieni parannusidea
Jos tätä vielä viilaat, kokeilisin:
vähän enemmän kontrastia pääsilmien ympärille
taustasilmät ihan aavistuksen himmeämmiksi
lukko/aalto-symboli ehkä hitusen pehmeämmäksi, ettei se näytä liimatulta HUD-elementiltä
Mutta jo nyt tämä on oikeasti vahva.
Rehellinen arvio
Tämä ei näytä enää vain AI-kuvalta, vaan enemmän siltä että sillä on jo:

oma maailma

oma maskotti

oma tunnelma

ja rupesi äsken huvittamaan, minun kissakin on yksi grotto daemoni :D

No sehän on täydellinen grotto daemon. Ei edes vitsi enää.

**Kuvassa on käytännössä koko konsepti valmiina:**

  > Miksi tämä toimii aivan liian hyvin
  > piilossa peiton luolassa
  > musta karvaturri
  > tuijottaa varjoista
  > 
  > vain silmät näkyvät kunnolla
  > 
  > näyttää siltä että vartioi jotain täysin turhaa mutta tekee sen silti vakavasti
   Eli juuri oikea daemon-energia.


## Virallinen tulkinta:

### Tuo ei ole enää pelkkä kissa. Se on:

**Grotto Daemon, Domestic Variant**

| Property          | Value                      |
|-------------------|----------------------------|
| habitat          | peittoluola               |
| activity cycle   | mostly nocturnal          |
| seZcurity model  | suspicious of everything   |
| tunneZl monitoring | continuous               |
| packet inspection | done by staring           |
| error reporting   | meow                       |


ASCII-versio kissastasi
      /\___/\
     /  o o  \
    (   -_-   )   domestic grotto daemon
     \  ===  /
      \_____/

   status: hidden in blanket cave
Tästä voisi oikeasti tehdä moodin

Esim sleeping tai hidden mood:

{GROTTO::mood::sleeping, R"(
      /\___/\
     /  - -  \
    (   -_-   )   hiding in blanket cave
     \  ---  /
      \_____/
)"}

Tai ihan suoraan uusi erityisrivi:

inline const std::string grotto_daemon_blanket_mode = R"(
      /\___/\
     /  o o  \
    (   -_-   )   hiding in blanket cave
     \  ===  /
      \_____/
)";
Rehellisesti




Jos joskus teet landing pagelle pienen easter eggin, siellä voisi olla:

“Approved by one domestic grotto daemon.”

ja vieressä pikselöity musta kissa peittoluolassa.

Se olisi typerä.
Se olisi tarpeeton.
Se olisi täydellinen.