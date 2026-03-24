# **GROTTO.chat landingpage**

[Project folder: `grotto-landing/`](grotto-landing/)


      > *End-to-end encrypted chat and voice application for > friend groups.
      > Combines irssi-style minimal terminal aesthetics with modern features:*

- Signal Protocol E2E encryption
- voice rooms
- file transfers
- link previews
- lightweight native clients

<div class="margin-right:20px"><extremely low resource usage and self-hostable alternative to Discord-style platforms.*

---

# **GROTTO.chat Landing Page Spec**

## Brand direction

GROTTO.chat should feel like a private encrypted cave for friend groups.

It is not a work chat, not a productivity dashboard, and not a crypto brand. The visual and verbal identity should combine:

* irssi-inspired terminal minimalism
* modern encrypted messaging credibility
* dark cave atmosphere
* subtle mascot/lore support via the grotto daemon
* warmth and intimacy for small groups

Core positioning:

**Private encrypted chat and voice for friend groups.**

Supporting brand line:

**Encrypted tunnels for friend groups.**

---

## Visual direction

### Mood

* dark-first
* underground, quiet, private
* technical but not sterile
* modern but not corporate
* slightly mysterious, never edgy-for-its-own-sake

### Color tokens

```css
:root {
  --bg-0: #070809;
  --bg-1: #0d1110;
  --bg-2: #141918;
  --surface-1: #171d1c;
  --surface-2: #1d2523;
  --surface-3: #26302d;

  --text-1: #eef2ef;
  --text-2: #c6d0cb;
  --text-3: #93a19a;
  --text-dim: #6c7a74;

  --line-soft: rgba(190, 220, 205, 0.10);
  --line-strong: rgba(190, 220, 205, 0.18);

  --accent: #8edb4f;
  --accent-strong: #a6f05d;
  --accent-dim: #5d8f38;

  --signal: #9eff57;
  --torch: #ffb347;
  --danger: #ff6b6b;

  --shadow-lg: 0 20px 60px rgba(0, 0, 0, 0.45);
  --shadow-glow: 0 0 40px rgba(142, 219, 79, 0.18);

  --radius-sm: 10px;
  --radius-md: 16px;
  --radius-lg: 24px;

  --font-ui: "Inter", "Segoe UI", sans-serif;
  --font-mono: "JetBrains Mono", "Cascadia Code", monospace;

  --space-1: 4px;
  --space-2: 8px;
  --space-3: 12px;
  --space-4: 16px;
  --space-5: 24px;
  --space-6: 32px;
  --space-7: 48px;
  --space-8: 64px;

  --max-width: 1200px;
}
```

### Typography

* Headings: modern sans-serif, clean and restrained
* Body copy: simple sans-serif, short paragraphs, no marketing fluff
* Mono usage: labels, technical callouts, status chips, tiny captions
* Do not set the entire page in mono unless the goal is to make users feel like they are trapped in a README file

### UI motifs

* subtle panel borders
* green signal glows used sparingly
* faint cave grain / dark texture
* terminal-like labels such as `secure tunnel ready`
* rounded panels, never glossy or overproduced

---

## Asset plan

### Existing assets to use

* `grotto-black.png` for dark hero use
* `grotto-transparent.png` for layered placement on dark backgrounds

### Additional assets recommended

1. Logo lockups

   * horizontal logo
   * icon-only logo
   * wordmark-only logo
   * small header logo for nav

2. Hero visual set

   * dark hero illustration with room for text
   * cropped transparent version for side panel use

3. Feature icon set

   * encryption
   * voice rooms
   * file transfer
   * admin TUI
   * public server directory
   * search

4. Daemon support assets

   * calm daemon
   * grumpy daemon
   * sleeping daemon
   * watching daemon

5. Background support

   * subtle cave grain texture
   * soft green signal glow
   * amber torch bloom for accent areas

### Daemon usage rule

The grotto daemon is a supporting mascot, not the main logo.

Use it in:

* installer
* onboarding
* docs
* loading / empty states
* footer easter egg
* occasional callout blocks

Do not let the mascot overpower the main product identity.

---

## Information architecture

Landing page order:

1. Header / nav
2. Hero
3. Why GROTTO
4. Features
5. Security
6. Hosting / self-hosting
7. Platform and client details
8. Final CTA
9. Footer

---

## Final page copy

## Header / Nav

Left:

* logo
* wordmark

Right:

* Features
* Security
* Host a server
* Docs
* Download

Primary nav CTA:

* **Download client**

Secondary nav CTA:

* **Host a server**

---

## Hero

### Eyebrow

`secure tunnel ready`

### Headline

**Private encrypted chat and voice for friend groups.**

### Supporting copy

GROTTO.chat combines irssi-like terminal aesthetics with modern end-to-end encryption, voice rooms, file transfers, and self-hosted servers.

### CTA row

* **Download client**
* **Host a server**

### Secondary line under CTAs

Signal Protocol, voice rooms, file transfer, search, themes, and an admin TUI.

### Hero side content options

Preferred:

* main logo / hero artwork on the right
* small terminal panel underneath with product status snippets

Suggested terminal panel copy:

```text
[ secure tunnel ready ]
[ voice caverns online ]
[ file transfer active ]
[ admin tui available ]
```

---

## Section: Why GROTTO

### Section title

**Built for smaller circles.**

### Intro copy

GROTTO.chat is for friend groups, private communities, and people who want a quieter place to talk. Not an engagement machine. Not a work dashboard. Just encrypted chat, voice, files, and servers for groups that prefer their own cave.

### Three-card layout

#### 1. Private by design

End-to-end encrypted messaging and calls, certificate pinning with TOFU, and platform-specific key protection where available.

#### 2. Built for real groups

Group chats, private calls, voice rooms, file transfer, search, and server discovery without turning your conversations into a product funnel.

#### 3. Terminal soul, modern comfort

Inspired by irssi and classic terminal clients, with modern usability features like themes, mouse support, notifications, and mobile security features.

---

## Section: Features

### Section title

**Everything your group needs. Nothing bloated.**

### Feature groups

#### Secure messaging

* End-to-end encryption via Signal Protocol
* X3DH + Double Ratchet
* Sender Keys for group chats
* Certificate pinning with TOFU

#### Voice and real-time communication

* Voice rooms
* Private calls
* WebRTC peer-to-peer media
* Push notifications on Android

#### Everyday usability

* File transfers with chunked upload/download
* Link previews
* Full-text message search with filters
* Mouse support in the desktop client
* Tokyo Night dark theme and clean light theme

#### Server administration

* Admin TUI for management
* Log viewer
* Settings and user management
* Public server directory for discovery

Suggested UI pattern:

* 4 groups in a two-column grid
* each group in a panel with mono subheading and 4 bullets max

---

## Section: Security

### Section title

**Built around real encryption, not branding theater.**

### Body copy

GROTTO.chat uses Signal Protocol primitives for end-to-end encrypted messaging, including X3DH for key agreement and Double Ratchet for forward secrecy. Group chats use Sender Keys for efficient multi-party encryption. Android clients can additionally use biometric protection for identity key access, SQLCipher for local database encryption, and screen capture protection.

### Security callout list

* Signal Protocol foundations
* Sender Keys for groups
* Certificate pinning with TOFU
* SQLCipher on Android
* Biometric identity key access on Android
* Screen capture protection on Android

### Suggested visual treatment

A darker panel with subtle signal-green linework and a mono label such as:

`cryptography: active`

---

## Section: Hosting

### Section title

**Run your own cave.**

### Body copy

Host a private server for your group, manage it through the admin TUI, and decide whether it stays private or appears in the public server directory. GROTTO.chat is designed to work well for small communities that want control without enterprise nonsense.

### Bullets

* Self-hosted server support
* Admin TUI with logs and settings
* Optional public server directory
* Friend-group scale first

### CTA row

* **Host a server**

* **Read the docs**

---

## Section: Clients and platforms

### Section title

**Terminal roots, modern clients.**

### Body copy

The desktop experience keeps the minimal, text-first feeling that made terminal chat clients great, while adding practical comforts like mouse support, search, resizing, themes, and richer media handling. Android support adds push notifications, biometric key protection, SQLCipher database encryption, and screen capture protection.

### Suggested split layout

Left:

* desktop / TUI emphasis

Right:

* Android security features

---

## Section: Product philosophy

### Section title

**A quieter place to talk.**

### Body copy

GROTTO.chat is built for people who want a more intentional communication space. It is not designed to maximize engagement, distract your group, or push everyone into a giant global feed. It is meant to feel smaller, calmer, and more yours.

Optional mono callout:

`not a feed`

`not a workspace`

`not a growth funnel`

`just your cave`

---

## Final CTA

### Headline

**Enter the cave.**

### Supporting copy

Download the client, host your own server, and build a quieter encrypted space for your group.

### Buttons

* **Download client**
* **Host a server**

### Small line underneath

Private chat, voice, files, and servers for friend groups.

---

## Footer

Footer columns:

### Product

* Download
* Features
* Security
* Server directory

### Docs

* Hosting
* Admin TUI
* Android security
* FAQ

### Community

* GitHub
* Status / updates

### Tiny mascot/easter egg

Small line:

**Approved by one domestic grotto daemon.**

Optional tiny blanket-cave cat pixel art or daemon face.

---

## Interaction notes

* hero artwork can have a slight floating effect, but keep it restrained
* buttons should feel tactile, not glossy
* hover states should use subtle border / glow changes only
* feature cards can brighten slightly on hover
* avoid heavy animation that fights the terminal-inspired vibe

---

## Layout notes for implementation

### Desktop

* max-width 1200px
* large two-column hero
* generous spacing between sections
* alternating section rhythm using dark surface panels

### Tablet

* collapse hero to stacked layout
* keep CTA row visible without overflow

### Mobile

* icon above headline
* one-column flow
* shorter copy blocks
* feature groups as stacked cards

---

## Writing rules

* keep paragraphs short
* avoid generic SaaS phrases
* avoid overdoing cave lore in core product copy
* keep the daemon mostly as flavor, not the main sales argument
* always prioritize clarity over gimmicks

---

## Optional extra section if needed later

### Installer / daemon section

Headline:

**Even the installer has tunnel energy.**

Body:

GROTTO.chat includes a terminal-inspired installer and supporting daemon identity that carry the same minimal, underground aesthetic across the whole project.

Use only if you want to lean into the product personality later. It should not be required in v1.




## Mikä tässä osuu

 - `grotto-daemon-cave.png`:

> Useampi silmäpari taustalla
> Tuo on erinomainen lisä. Heti tulee olo, että daemon ei ole vain yksi otus vaan ehkä koko luolan verkosto tai parvi. Se sopii täydellisesti salattuihin tunneleihin ja taustalla pyörivään järjestelmään.

> Tummempi keskusta
> Nyt katse menee kunnolla noihin hehkuviin silmiin. Aiempi versio oli vähän “hahmo poseeraa”. Tämä on enemmän “hahmo vartioi ja tarkkailee”. Selvästi parempi.
>
> *'Vihreä hehku kristallissa** Toimii edelleen todella hyvin salausavaimen symbolina. Tuo on melkein valmis brändielementti jo.
> 
> Luolan syvyys
> Photoshoppaus teki tästä enemmän paikan kuin kuvituksen. Nyt tämä näyttää siltä että luolassa on oikeasti käytäviä, ei vain yksi kivinen kaari.
> 
> Mikä tästä tulee mieleen brändin kannalta
> 
> Tämä voisi toimia jopa kolmella tasolla:
> 
### 1. Installer / splash art

Tähän se sopii heti. Todella hyvin.

### 2. Landing page hero tausta

Varsinkin tummalla overlayllä ja tekstillä päälle:
Encrypted tunnels for friend groups
Private chat, voice and file transfer inside the cave


---

Asset usage guidance for GROTTO.chat:

Use the brand assets with clear hierarchy:

1. Primary identity
- The main GROTTO.chat logo / logo lockup is the primary hero brand asset.
- Keep it as the main recognizable identity in the header and hero.
- Do not replace the main product identity with the daemon mascot.

2. Supporting atmosphere
- Use the cave/tunnel illustration as an atmospheric supporting visual.
- Best placement: Hosting section (“Run your own cave”) or a dark supporting panel.
- It can also be used subtly in background composition, but should not overpower content.

3. Supporting mascot
- Use the daemon portrait only as a secondary/supporting visual.
- Best placement: Security section, onboarding-style supporting card, or a subtle callout.
- Limit daemon usage to 1–2 appearances on the page maximum.
- Do not use the daemon as the main hero identity.
- Do not make the page feel mascot-driven.

4. Footer easter egg
- A subtle footer easter egg is allowed if it fits the tone:
  “Approved by one domestic grotto daemon.”
- Keep it understated and tasteful.
- Do not let the joke reduce trust in the product.

5. General asset rules
- On any given section, prefer one main visual focus only.
- Avoid stacking logo, cave art, and daemon art all as equally dominant elements in the same viewport.
- Use restraint. The page should feel atmospheric, credible, and product-focused.

Suggested v1 landing page asset placement:
- Hero: main GROTTO logo / lockup
- Security: daemon portrait
- Hosting: cave / tunnel illustration
- Footer: subtle daemon/cat easter egg if appropriate

If asset files need to be moved into a public/assets location to work in production, do so cleanly and update references accordingly.


# **GROTTO.chat Landing Page Spec**

## Brand direction

GROTTO.chat should feel like a private encrypted cave for friend groups.

It is not a work chat, not a productivity dashboard, and not a crypto brand. The visual and verbal identity should combine:

* irssi-inspired terminal minimalism
* modern encrypted messaging credibility
* dark cave atmosphere
* subtle mascot/lore support via the grotto daemon
* warmth and intimacy for small groups

Core positioning:

**Private encrypted chat and voice for friend groups.**

Supporting brand line:

**Encrypted tunnels for friend groups.**

---

## Visual direction

### Mood

* dark-first
* underground, quiet, private
* technical but not sterile
* modern but not corporate
* slightly mysterious, never edgy-for-its-own-sake

### Color tokens

```css
:root {
  --bg-0: #070809;
  --bg-1: #0d1110;
  --bg-2: #141918;
  --surface-1: #171d1c;
  --surface-2: #1d2523;
  --surface-3: #26302d;

  --text-1: #eef2ef;
  --text-2: #c6d0cb;
  --text-3: #93a19a;
  --text-dim: #6c7a74;

  --line-soft: rgba(190, 220, 205, 0.10);
  --line-strong: rgba(190, 220, 205, 0.18);

  --accent: #8edb4f;
  --accent-strong: #a6f05d;
  --accent-dim: #5d8f38;

  --signal: #9eff57;
  --torch: #ffb347;
  --danger: #ff6b6b;

  --shadow-lg: 0 20px 60px rgba(0, 0, 0, 0.45);
  --shadow-glow: 0 0 40px rgba(142, 219, 79, 0.18);

  --radius-sm: 10px;
  --radius-md: 16px;
  --radius-lg: 24px;

  --font-ui: "Inter", "Segoe UI", sans-serif;
  --font-mono: "JetBrains Mono", "Cascadia Code", monospace;

  --space-1: 4px;
  --space-2: 8px;
  --space-3: 12px;
  --space-4: 16px;
  --space-5: 24px;
  --space-6: 32px;
  --space-7: 48px;
  --space-8: 64px;

  --max-width: 1200px;
}
```

### Typography

* Headings: modern sans-serif, clean and restrained
* Body copy: simple sans-serif, short paragraphs, no marketing fluff
* Mono usage: labels, technical callouts, status chips, tiny captions
* Do not set the entire page in mono unless the goal is to make users feel like they are trapped in a README file

### UI motifs

* subtle panel borders
* green signal glows used sparingly
* faint cave grain / dark texture
* terminal-like labels such as `secure tunnel ready`
* rounded panels, never glossy or overproduced

---

## Asset plan

### Existing assets to use

* `grotto-black.png` for dark hero use
* `grotto-transparent.png` for layered placement on dark backgrounds

### Additional assets recommended

1. Logo lockups

   * horizontal logo
   * icon-only logo
   * wordmark-only logo
   * small header logo for nav

2. Hero visual set

   * dark hero illustration with room for text
   * cropped transparent version for side panel use

3. Feature icon set

   * encryption
   * voice rooms
   * file transfer
   * admin TUI
   * public server directory
   * search

4. Daemon support assets

   * calm daemon
   * grumpy daemon
   * sleeping daemon
   * watching daemon

5. Background support

   * subtle cave grain texture
   * soft green signal glow
   * amber torch bloom for accent areas

### Daemon usage rule

The grotto daemon is a supporting mascot, not the main logo.

Use it in:

* installer
* onboarding
* docs
* loading / empty states
* footer easter egg
* occasional callout blocks

Do not let the mascot overpower the main product identity.

---

## Information architecture

Landing page order:

1. Header / nav
2. Hero
3. Why GROTTO
4. Features
5. Security
6. Hosting / self-hosting
7. Platform and client details
8. Final CTA
9. Footer

---

## Final page copy

## Header / Nav

Left:

* logo
* wordmark

Right:

* Features
* Security
* Host a server
* Docs
* Download

Primary nav CTA:

* **Download client**

Secondary nav CTA:

* **Host a server**

---

## Hero

### Eyebrow

`secure tunnel ready`

### Headline

**Private encrypted chat and voice for friend groups.**

### Supporting copy

GROTTO.chat combines irssi-like terminal aesthetics with modern end-to-end encryption, voice rooms, file transfers, and self-hosted servers.

### CTA row

* **Download client**
* **Host a server**

### Secondary line under CTAs

Signal Protocol, voice rooms, file transfer, search, themes, and an admin TUI.

### Hero side content options

Preferred:

* main logo / hero artwork on the right
* small terminal panel underneath with product status snippets

Suggested terminal panel copy:

```text
[ secure tunnel ready ]
[ voice caverns online ]
[ file transfer active ]
[ admin tui available ]
```

---

## Section: Why GROTTO

### Section title

**Built for smaller circles.**

### Intro copy

GROTTO.chat is for friend groups, private communities, and people who want a quieter place to talk. Not an engagement machine. Not a work dashboard. Just encrypted chat, voice, files, and servers for groups that prefer their own cave.

### Three-card layout

#### 1. Private by design

End-to-end encrypted messaging and calls, certificate pinning with TOFU, and platform-specific key protection where available.

#### 2. Built for real groups

Group chats, private calls, voice rooms, file transfer, search, and server discovery without turning your conversations into a product funnel.

#### 3. Terminal soul, modern comfort

Inspired by irssi and classic terminal clients, with modern usability features like themes, mouse support, notifications, and mobile security features.

---

## Section: Features

### Section title

**Everything your group needs. Nothing bloated.**

### Feature groups

#### Secure messaging

* End-to-end encryption via Signal Protocol
* X3DH + Double Ratchet
* Sender Keys for group chats
* Certificate pinning with TOFU

#### Voice and real-time communication

* Voice rooms
* Private calls
* WebRTC peer-to-peer media
* Push notifications on Android

#### Everyday usability

* File transfers with chunked upload/download
* Link previews
* Full-text message search with filters
* Mouse support in the desktop client
* Tokyo Night dark theme and clean light theme

#### Server administration

* Admin TUI for management
* Log viewer
* Settings and user management
* Public server directory for discovery

Suggested UI pattern:

* 4 groups in a two-column grid
* each group in a panel with mono subheading and 4 bullets max

---

## Section: Security

### Section title

**Built around real encryption, not branding theater.**

### Body copy

GROTTO.chat uses Signal Protocol primitives for end-to-end encrypted messaging, including X3DH for key agreement and Double Ratchet for forward secrecy. Group chats use Sender Keys for efficient multi-party encryption. Android clients can additionally use biometric protection for identity key access, SQLCipher for local database encryption, and screen capture protection.

### Security callout list

* Signal Protocol foundations
* Sender Keys for groups
* Certificate pinning with TOFU
* SQLCipher on Android
* Biometric identity key access on Android
* Screen capture protection on Android

### Suggested visual treatment

A darker panel with subtle signal-green linework and a mono label such as:

`cryptography: active`

---

## Section: Hosting

### Section title

**Run your own cave.**

### Body copy

Host a private server for your group, manage it through the admin TUI, and decide whether it stays private or appears in the public server directory. GROTTO.chat is designed to work well for small communities that want control without enterprise nonsense.

### Bullets

* Self-hosted server support
* Admin TUI with logs and settings
* Optional public server directory
* Friend-group scale first

### CTA row

* **Host a server**
* **Read the docs**

---

## Section: Clients and platforms

### Section title

**Terminal roots, modern clients.**

### Body copy

The desktop experience keeps the minimal, text-first feeling that made terminal chat clients great, while adding practical comforts like mouse support, search, resizing, themes, and richer media handling. Android support adds push notifications, biometric key protection, SQLCipher database encryption, and screen capture protection.

### Suggested split layout

Left:

* desktop / TUI emphasis

Right:

* Android security features

---

## Section: Product philosophy

### Section title

**A quieter place to talk.**

### Body copy

GROTTO.chat is built for people who want a more intentional communication space. It is not designed to maximize engagement, distract your group, or push everyone into a giant global feed. It is meant to feel smaller, calmer, and more yours.

Optional mono callout:

`not a feed`
`not a workspace`
`not a growth funnel`
`just your cave`

---

## Final CTA

### Headline

**Enter the cave.**

### Supporting copy

Download the client, host your own server, and build a quieter encrypted space for your group.

### Buttons

* **Download client**
* **Host a server**

### Small line underneath

Private chat, voice, files, and servers for friend groups.

---

## Footer

Footer columns:

### Product

* Download
* Features
* Security
* Server directory

### Docs

* Hosting
* Admin TUI
* Android security
* FAQ

### Community

* GitHub
* Status / updates

### Tiny mascot/easter egg

Small line:

**Approved by one domestic grotto daemon.**

Optional tiny blanket-cave cat pixel art or daemon face.

---

## Interaction notes

* hero artwork can have a slight floating effect, but keep it restrained
* buttons should feel tactile, not glossy
* hover states should use subtle border / glow changes only
* feature cards can brighten slightly on hover
* avoid heavy animation that fights the terminal-inspired vibe

---

## Layout notes for implementation

### Desktop

* max-width 1200px
* large two-column hero
* generous spacing between sections
* alternating section rhythm using dark surface panels

### Tablet

* collapse hero to stacked layout
* keep CTA row visible without overflow

### Mobile

* icon above headline
* one-column flow
* shorter copy blocks
* feature groups as stacked cards

---

## Writing rules

* keep paragraphs short
* avoid generic SaaS phrases
* avoid overdoing cave lore in core product copy
* keep the daemon mostly as flavor, not the main sales argument
* always prioritize clarity over gimmicks

---

## Optional extra section if needed later

### Installer / daemon section

Headline:

**Even the installer has tunnel energy.**

Body:

GROTTO.chat includes a terminal-inspired installer and supporting daemon identity that carry the same minimal, underground aesthetic across the whole project.
Use only if you want to lean into the product personality later. It should not be required in v1.

