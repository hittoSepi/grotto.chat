# IrssiCord Plugin System — PLAN

**Stack:** C++20 · QuickJS · Boost.Asio · Protobuf
**Lähestymistapa:** Yksi JSRuntime per plugin, botit omina client-instansseina

---

## 1. Yleiskuva

IrssiCordin plugin-systeemi mahdollistaa kolmannen osapuolen laajennukset kolmella tasolla:
botit (omat client-instanssit), client-laajennukset (käyttäjän prosessissa),
ja server-laajennukset (serverin prosessissa). Scriptikielenä QuickJS (ES2023).

E2E-salaus asettaa arkkitehtuurin keskeisen reunaehdon: **server ei koskaan näe
viestien sisältöä**. Tästä seuraa, että viestisisältöön reagoivat pluginit
(botit, slash-komennot) toteutetaan client-puolella tai omina client-instansseina.

```
grotto-server
    │
    ├── Session: Sepi          ← oikea käyttäjä (notcurses TUI)
    ├── Session: Matti         ← oikea käyttäjä
    ├── Session: DiceBot       ← bot-plugin, oma Ed25519-identiteetti
    └── Session: WelcomeBot    ← bot-plugin
```

---

## 2. Plugin-tyypit

### 2.1 `bot` — Oma client-instanssi

Botti on headless grotto-client: NetClient + CryptoEngine ilman UI-kerrosta.
Se autentikoi omalla Ed25519-avaimellaan, liittyy kanaville, vastaanottaa
ja lähettää viestejä kuten normaali käyttäjä.

**Käyttötapaukset:**
- Slash-komentobotit (/roll, /poll, /remind)
- Automaattiset vastaukset (FAQ-botti)
- Integraatiobotit (sää, RSS, CI/CD-ilmoitukset)

**Näkee:** plaintext-viestit, kanava-tapahtumat, presence
**Voi:** lähettää viestejä, liittyä/poistua kanavilta, rekisteröidä slash-komentoja

### 2.2 `client_extension` — Käyttäjän clientin laajennus

Pyörii käyttäjän omassa client-prosessissa. Ei omaa yhteyttä serverille,
käyttää host-clientin NetClientiä. Plugin on aktiivinen vain kun käyttäjä on online.

**Käyttötapaukset:**
- Lokaalit slash-komennot (/clear, /theme, /export)
- Viestin muokkaus ennen näyttöä (syntax highlight, emoji-korvaus)
- Automaattinen tab-complete laajennus
- Notifikaatiosäännöt

**Näkee:** plaintext-viestit (käyttäjän sessio), UI-tapahtumat
**Voi:** lisätä slash-komentoja, muokata näkyvää viestisisältöä, käyttää localStorea

### 2.3 `server_extension` — Serverin laajennus

Pyörii serverin prosessissa. Näkee **vain metadatan**, ei koskaan viestisisältöä.

**Käyttötapaukset:**
- Automaattinen kanavanhallinta (luo/poista ajastetut kanavat)
- Räätälöity rate limiting
- Kutsujärjestelmän laajennus
- Audit-lokitus (kuka liittyi, kuka lähti, milloin)
- Automaattinen kick/ban-logiikka (perustuu metadataan, ei viestisisältöön)

**Näkee:** yhteystapahtumia, presence, kanava-join/part, voice-tapahtumat
**Ei näe:** viestien sisältöä (E2E-salattu)

---

## 3. Hakemistorakenne levyllä

### 3.1 Plugin-hakemistot

```
# Server-puoli
grotto-server/
└── plugins/
    ├── channel-logger/
    │   ├── plugin.json
    │   └── main.js
    └── auto-kick/
        ├── plugin.json
        └── main.js

# Client-puoli (käyttäjän config-hakemisto)
~/.config/grotto/
└── plugins/
    ├── dicebot/
    │   ├── plugin.json
    │   ├── main.js
    │   └── config.toml
    ├── welcomebot/
    │   ├── plugin.json
    │   └── main.js
    └── syntax-highlight/
        ├── plugin.json
        └── main.js
```

### 3.2 Plugin-paketin sisältö

```
dicebot/
├── plugin.json        ← pakollinen: metadata
├── main.js            ← pakollinen: entry point
├── config.toml        ← valinnainen: pluginin oma konfiguraatio
├── lib/               ← valinnainen: lisä JS-moduulit
│   └── utils.js
└── data/              ← valinnainen: pluginin pysyvä data (runtime)
    └── state.json
```

### 3.3 plugin.json — Metadata

```json
{
  "name": "dicebot",
  "display_name": "Dice Bot",
  "version": "1.0.0",
  "type": "bot",
  "entry": "main.js",
  "description": "Rolls dice with /roll command",
  "min_host_version": "0.1.0",

  "bot": {
    "user_id": "DiceBot",
    "auto_join_channels": ["#general", "#games"]
  },

  "commands": [
    {
      "name": "/roll",
      "description": "Roll NdN dice (e.g. /roll 2d6)",
      "usage": "/roll [NdN]"
    }
  ],

  "permissions": [
    "send_messages",
    "read_messages",
    "register_commands"
  ]
}
```

**type-kenttä:** `"bot"` | `"client_extension"` | `"server_extension"`

**permissions-kenttä** — hallitsee mitä plugin saa tehdä:

| Permission            | bot | client_ext | server_ext | Kuvaus                          |
|-----------------------|-----|------------|------------|---------------------------------|
| `read_messages`       | ✓   | ✓          | ✗          | Lue plaintext-viestit           |
| `send_messages`       | ✓   | ✓          | ✗          | Lähetä viestejä kanavalle       |
| `register_commands`   | ✓   | ✓          | ✗          | Rekisteröi slash-komentoja      |
| `manage_channels`     | ✗   | ✗          | ✓          | Luo/poista kanavia              |
| `manage_users`        | ✗   | ✗          | ✓          | Kick/ban                        |
| `http_request`        | ✓   | ✓          | ✓          | Tee HTTP-pyyntöjä ulkoisiin API |
| `file_read`           | ✓   | ✓          | ✓          | Lue tiedostoja pluginin hakemistosta |
| `file_write`          | ✓   | ✓          | ✓          | Kirjoita pluginin data/ -hakemistoon |

---

## 4. Luokkahierarkia (C++)

### 4.1 Kokonaisrakenne

```
PluginManager
│
├── discover(plugins_dir)       — skannaa ja lataa pluginit
├── load(plugin_dir)            — lataa yksittäinen plugin
├── unload(name)                — pura plugin
├── reload(name)                — unload + load (hot-reload)
├── dispatch(PluginEvent)       — jaa eventti plugineille
├── dispatch_command(PluginCommand) — slash-komento plugineille
│
├── PluginInstance (abstrakti base)
│   ├── JSRuntime*              — oma QuickJS runtime (sandbox)
│   ├── JSContext*
│   ├── PluginMeta              — parsittu plugin.json
│   ├── EventRegistry           — "message" → [JS callback, ...]
│   ├── CommandRegistry         — "/roll" → JS callback
│   ├── init()                  — lataa scripti, setup API
│   ├── on_event(PluginEvent)   — dispatch JS-callbackeille
│   ├── on_command(PluginCommand)
│   ├── tick()                  — aja pending JS microtaskit
│   └── shutdown()
│   │
│   ├── BotPluginInstance : PluginInstance
│   │   └── BotClient           — headless NetClient + CryptoEngine
│   │       ├── connect()
│   │       ├── send_message(channel, text)
│   │       ├── join_channel(channel)
│   │       └── leave_channel(channel)
│   │
│   ├── ClientExtPlugin : PluginInstance
│   │   └── host_client_ (viite isännöivään clienttiin)
│   │
│   └── ServerExtPlugin : PluginInstance
│       └── host_server_ (viite isännöivään serveriin)
│
└── PluginContext                — jaettu konteksti
    ├── io_context&             — Boost.Asio event loop
    ├── config&                 — host-sovelluksen config
    └── logger                  — spdlog instance
```

### 4.2 PluginManager

```cpp
class PluginManager {
public:
    explicit PluginManager(PluginContext& ctx);
    ~PluginManager();

    // ── Elinkaari ───────────────────────────────────────
    // Skannaa hakemisto ja lataa kaikki löydetyt pluginit
    void discover(const std::filesystem::path& plugins_dir);

    // Yksittäisen pluginin hallinta
    error_code load(const std::filesystem::path& plugin_dir);
    void       unload(std::string_view name);
    error_code reload(std::string_view name);

    // ── Event dispatch ──────────────────────────────────
    // Kutsutaan host-prosessista kun tapahtuma tulee
    void dispatch(const PluginEvent& event);

    // Slash-komento saapui
    void dispatch_command(const PluginCommand& cmd);

    // ── QuickJS event loop integraatio ──────────────────
    // Kutsutaan Asio event loopista säännöllisesti
    // Ajaa kaikkien pluginien pending JS microtaskit
    void tick();

    // ── Introspektio ────────────────────────────────────
    std::vector<PluginInfo> loaded_plugins() const;
    bool is_loaded(std::string_view name) const;
    std::vector<CommandInfo> registered_commands() const;

private:
    PluginContext& ctx_;
    std::unordered_map<std::string, std::unique_ptr<PluginInstance>> plugins_;
    mutable std::shared_mutex mu_;

    // Globaali komento → plugin mapping (nopea lookup)
    std::unordered_map<std::string, PluginInstance*> command_map_;

    // Plugin.json parsinta ja validointi
    std::optional<PluginMeta> parse_meta(const std::filesystem::path& json_path);
    bool validate_meta(const PluginMeta& meta);
};
```

### 4.3 PluginInstance (abstrakti base)

```cpp
class PluginInstance {
public:
    explicit PluginInstance(PluginMeta meta, PluginContext& ctx);
    virtual ~PluginInstance();

    // ── Elinkaari ───────────────────────────────────────
    error_code init();           // Lataa script, setup API, aja entry
    virtual void shutdown() = 0; // Siivoa resurssit, kutsuu grotto.onShutdown

    // ── Eventit ─────────────────────────────────────────
    virtual void on_event(const PluginEvent& event);
    virtual void on_command(const PluginCommand& cmd);

    // ── QuickJS tick ────────────────────────────────────
    void tick();                 // JS_ExecutePendingJob

    // ── Getterit ────────────────────────────────────────
    const PluginMeta& meta() const;
    PluginState       state() const;  // Loading, Running, Error, Stopped

protected:
    // ── QuickJS API setup ───────────────────────────────
    void setup_js_api();         // Luo grotto-globaali + funktiot
    virtual void setup_type_specific_api() = 0; // Tyyppikohtaiset lisäykset

    // ── JS-funktioiden kutsuminen ───────────────────────
    // Try/catch wrapper — pluginin virhe ei kaada hostia
    bool call_js_safe(JSValue fn, std::span<JSValue> args);

    // ── Plugin-tason rekisteröinti (kutsutaan JS:stä) ───
    void register_event_handler(const std::string& event, JSValue fn);
    void register_command_handler(const std::string& cmd, JSValue fn);

    // ── Tila ────────────────────────────────────────────
    PluginMeta    meta_;
    PluginContext& host_ctx_;
    PluginState   state_ = PluginState::Loading;

    JSRuntime*    rt_  = nullptr;   // Oma runtime — täysi sandbox
    JSContext*    js_  = nullptr;

    // Event handlers: "message" → [fn1, fn2, ...]
    std::unordered_map<std::string, std::vector<JSValue>> event_handlers_;

    // Command handlers: "/roll" → fn
    std::unordered_map<std::string, JSValue> command_handlers_;

    // Virheiden seuranta — liikaa virheitä → automaattinen unload
    int   error_count_ = 0;
    static constexpr int kMaxErrorsBeforeUnload = 50;
};
```

### 4.4 BotPluginInstance

```cpp
class BotPluginInstance : public PluginInstance {
public:
    BotPluginInstance(PluginMeta meta, PluginContext& ctx);
    ~BotPluginInstance() override;

    void shutdown() override;

    // BotClient lähettää vastaanotetut viestit tänne
    // → purku → dispatch JS-callbackeille
    void on_message_received(const ChatEnvelope& env);

protected:
    void setup_type_specific_api() override;

private:
    // ── Headless client ─────────────────────────────────
    // Jakaa net/ ja crypto/ -koodin oikean clientin kanssa
    std::unique_ptr<BotClient> bot_;

    // C-funktiot jotka exposataan QuickJS:lle
    // grotto.sendMessage(channel, text)
    static JSValue js_send_message(JSContext* ctx, JSValue this_val,
                                    int argc, JSValue* argv);
    // grotto.bot.joinChannel(channel)
    static JSValue js_join_channel(JSContext* ctx, JSValue this_val,
                                    int argc, JSValue* argv);
    // grotto.bot.leaveChannel(channel)
    static JSValue js_leave_channel(JSContext* ctx, JSValue this_val,
                                     int argc, JSValue* argv);
};
```

### 4.5 ClientExtPlugin

```cpp
class ClientExtPlugin : public PluginInstance {
public:
    ClientExtPlugin(PluginMeta meta, PluginContext& ctx,
                    AppState& app_state, NetClient& net);
    ~ClientExtPlugin() override;

    void shutdown() override;

protected:
    void setup_type_specific_api() override;

private:
    AppState&  app_state_;   // Käyttäjän client-tila (read + UI post)
    NetClient& net_;         // Viestien lähetys käyttäjän sessiona

    // JS API: grotto.ui.notify(text), grotto.ui.setTheme(name), ...
    static JSValue js_send_message(JSContext*, JSValue, int, JSValue*);
    static JSValue js_get_active_channel(JSContext*, JSValue, int, JSValue*);
    static JSValue js_get_online_users(JSContext*, JSValue, int, JSValue*);
};
```

### 4.6 ServerExtPlugin

```cpp
class ServerExtPlugin : public PluginInstance {
public:
    ServerExtPlugin(PluginMeta meta, PluginContext& ctx,
                    ChannelManager& channels, SessionManager& sessions);
    ~ServerExtPlugin() override;

    void shutdown() override;

    // Override: suodata pois plaintext-eventit
    void on_event(const PluginEvent& event) override;

protected:
    void setup_type_specific_api() override;

private:
    ChannelManager& channels_;
    SessionManager& sessions_;

    // JS API: grotto.server.kickUser(user, reason), ...
    static JSValue js_kick_user(JSContext*, JSValue, int, JSValue*);
    static JSValue js_create_channel(JSContext*, JSValue, int, JSValue*);
    static JSValue js_delete_channel(JSContext*, JSValue, int, JSValue*);
    static JSValue js_get_online_users(JSContext*, JSValue, int, JSValue*);
};
```

---

## 5. Event-järjestelmä

### 5.1 PluginEvent

```cpp
struct PluginEvent {
    enum class Type {
        // ── Kaikille plugin-tyypeille ───────────────────
        UserJoined,              // käyttäjä liittyi kanavalle
        UserLeft,                // käyttäjä lähti kanavalta
        PresenceChanged,         // online/away/offline
        ChannelCreated,
        ChannelDeleted,

        // ── Vain bot + client_extension ─────────────────
        MessageReceived,         // plaintext saatavilla

        // ── Vain server_extension ───────────────────────
        ConnectionEstablished,   // uusi TLS-yhteys
        ConnectionDropped,       // yhteys katkesi
        VoiceRoomJoined,
        VoiceRoomLeft,
        AuthSuccess,             // käyttäjä autentikoitui
        AuthFailure,             // autentikointi epäonnistui
    };

    Type        type;
    std::string channel;         // "#general" tai ""
    std::string user_id;         // kuka aiheutti eventin
    std::string text;            // plaintext — tyhjä server_extension:lle
    int64_t     timestamp_ms;

    // Lisätiedot tyyppikohtaisesti
    std::unordered_map<std::string, std::string> extra;
};
```

### 5.2 PluginCommand

```cpp
struct PluginCommand {
    std::string name;            // "/roll"
    std::string channel;         // missä kanavalla annettiin
    std::string sender_id;       // kuka antoi komennon
    std::vector<std::string> args;  // "2d6" → ["2d6"]
    int64_t     timestamp_ms;
};
```

### 5.3 Dispatch-logiikka

```cpp
void PluginManager::dispatch(const PluginEvent& event) {
    std::shared_lock lock(mu_);

    for (auto& [name, plugin] : plugins_) {
        // Turvallisuussuodin: server_extension ei saa plaintext-eventtejä
        if (plugin->meta().type == PluginType::ServerExtension &&
            event.type == PluginEvent::Type::MessageReceived) {
            continue;
        }

        // Ajetaan pluginin omassa strandissa — ei blokkaa muita plugineja
        asio::post(plugin->strand(), [&plugin, event] {
            plugin->on_event(event);
        });
    }
}

void PluginManager::dispatch_command(const PluginCommand& cmd) {
    std::shared_lock lock(mu_);

    auto it = command_map_.find(cmd.name);
    if (it == command_map_.end()) return;

    PluginInstance* plugin = it->second;
    asio::post(plugin->strand(), [plugin, cmd] {
        plugin->on_command(cmd);
    });
}
```

---

## 6. QuickJS API — Scriptipuoli

### 6.1 Yhteinen API (kaikki plugin-tyypit)

```js
// ── Eventit ─────────────────────────────────────────────
grotto.on(event_name, callback)
// event_name: "message", "user_joined", "user_left",
//             "presence_changed", "shutdown"

// ── Komennot ────────────────────────────────────────────
grotto.onCommand(name, options, callback)
// name:     "/roll"
// options:  { description: "Roll dice", usage: "/roll [NdN]" }
// callback: (ctx) => { ctx.sender, ctx.channel, ctx.args }

// ── Viestit ─────────────────────────────────────────────
grotto.sendMessage(channel, text)

// ── Konfiguraatio ───────────────────────────────────────
grotto.getConfig()          // palauttaa pluginin config.toml objektina
grotto.getPluginDir()       // polku pluginin hakemistoon
grotto.getDataDir()         // polku pluginin data/ -hakemistoon

// ── Logging ─────────────────────────────────────────────
grotto.log.info(msg)        // → spdlog [PLUGIN:dicebot] msg
grotto.log.warn(msg)
grotto.log.error(msg)
grotto.log.debug(msg)

// ── Ajastus ─────────────────────────────────────────────
grotto.setTimeout(callback, delay_ms)
grotto.setInterval(callback, interval_ms)
grotto.clearTimeout(id)
grotto.clearInterval(id)

// ── HTTP (valinnainen, vaatii http_request permissionin) ─
grotto.http.get(url, options)      // → Promise<Response>
grotto.http.post(url, body, options)

// ── Tiedostot (pluginin oma data/) ──────────────────────
grotto.fs.readFile(relative_path)  // → Promise<string>
grotto.fs.writeFile(relative_path, content)
// HUOM: polut rajattu pluginin omaan data/ -hakemistoon
```

### 6.2 Bot-kohtainen API

```js
grotto.bot.userId                  // "DiceBot" — botin oma user_id
grotto.bot.joinChannel(channel)    // → Promise<void>
grotto.bot.leaveChannel(channel)   // → Promise<void>
grotto.bot.setPresence(status)     // "online" | "away"
```

### 6.3 Client Extension -kohtainen API

```js
grotto.client.activeChannel()      // → string
grotto.client.onlineUsers()        // → string[]
grotto.client.channels()           // → string[]

// UI-muokkaukset
grotto.client.notify(text)         // Näytä notifikaatio status barissa
```

### 6.4 Server Extension -kohtainen API

```js
grotto.server.kickUser(user_id, reason)
grotto.server.banUser(user_id, reason)
grotto.server.createChannel(channel_id)
grotto.server.deleteChannel(channel_id)
grotto.server.onlineUsers()        // → string[]
grotto.server.channelMembers(channel_id)  // → string[]
```

---

## 7. QuickJS integraatio C++:aan

### 7.1 Runtime-elinkaari

```cpp
error_code PluginInstance::init() {
    // 1. Luo oma runtime
    rt_ = JS_NewRuntime();
    if (!rt_) return error_code::RUNTIME_INIT_FAILED;

    // Muistiraja per plugin — estää yhtä pluginia syömästä kaikkea
    JS_SetMemoryLimit(rt_, 16 * 1024 * 1024);  // 16 MB per plugin

    // Max stack size
    JS_SetMaxStackSize(rt_, 1024 * 1024);       // 1 MB stack

    // 2. Luo context
    js_ = JS_NewContext(rt_);
    if (!js_) return error_code::CONTEXT_INIT_FAILED;

    // 3. Setup grotto-globaali
    setup_js_api();
    setup_type_specific_api();

    // 4. Lataa ja aja entry point -scripti
    auto script = read_file(meta_.plugin_dir / meta_.entry);
    if (!script) return error_code::SOURCE_FILE_NOT_FOUND;

    JSValue result = JS_Eval(js_, script->c_str(), script->size(),
                              meta_.entry.c_str(), JS_EVAL_TYPE_MODULE);
    if (JS_IsException(result)) {
        log_js_exception();
        JS_FreeValue(js_, result);
        return error_code::SCRIPT_INIT_FAILED;
    }
    JS_FreeValue(js_, result);

    state_ = PluginState::Running;
    return error_code::NONE;
}
```

### 7.2 Tick — JS microtask integraatio Asio event loopiin

QuickJS:llä ei ole omaa event loopia. Pending Promise-resoluutiot ja
setTimeout-callbackit ajetaan `tick()`:ssä, joka kutsutaan Asio-timerista.

```cpp
void PluginManager::start_tick_timer() {
    tick_timer_.expires_after(std::chrono::milliseconds(16)); // ~60 Hz
    tick_timer_.async_wait([this](auto ec) {
        if (ec) return;

        std::shared_lock lock(mu_);
        for (auto& [name, plugin] : plugins_) {
            plugin->tick();
        }

        start_tick_timer(); // reschedule
    });
}

void PluginInstance::tick() {
    // Aja kaikki pending microtaskit (Promise then/catch, queueMicrotask)
    JSContext* ctx;
    while (JS_ExecutePendingJob(rt_, &ctx) > 0) {}

    // Tarkista setTimeout/setInterval -ajastimet
    process_timers();
}
```

### 7.3 C++ → JS: Eventin välitys

```cpp
void PluginInstance::on_event(const PluginEvent& event) {
    auto event_name = event_type_to_string(event.type);
    auto it = event_handlers_.find(event_name);
    if (it == event_handlers_.end()) return;

    // Rakennetaan JS-objekti eventtiin
    JSValue js_event = JS_NewObject(js_);
    JS_SetPropertyStr(js_, js_event, "type",
        JS_NewString(js_, event_name.c_str()));
    JS_SetPropertyStr(js_, js_event, "channel",
        JS_NewString(js_, event.channel.c_str()));
    JS_SetPropertyStr(js_, js_event, "sender",
        JS_NewString(js_, event.user_id.c_str()));
    JS_SetPropertyStr(js_, js_event, "text",
        JS_NewString(js_, event.text.c_str()));
    JS_SetPropertyStr(js_, js_event, "timestamp",
        JS_NewFloat64(js_, static_cast<double>(event.timestamp_ms)));

    // Kutsutaan jokaista rekisteröityä handleria
    JSValue args[] = { js_event };
    for (JSValue& handler : it->second) {
        call_js_safe(handler, {args, 1});
    }

    JS_FreeValue(js_, js_event);
}
```

### 7.4 JS → C++: grotto.on() binding

```cpp
// Kutsutaan kun JS ajaa: grotto.on("message", fn)
static JSValue js_grotto_on(JSContext* ctx, JSValue this_val,
                             int argc, JSValue* argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "on() requires 2 arguments");

    const char* event_name = JS_ToCString(ctx, argv[0]);
    if (!event_name) return JS_EXCEPTION;

    JSValue fn = argv[1];
    if (!JS_IsFunction(ctx, fn)) {
        JS_FreeCString(ctx, event_name);
        return JS_ThrowTypeError(ctx, "on() second argument must be a function");
    }

    // Hae PluginInstance* opaque pointerista
    auto* plugin = static_cast<PluginInstance*>(JS_GetContextOpaque(ctx));

    // Duplikoi JSValue (reference count)
    plugin->register_event_handler(event_name, JS_DupValue(ctx, fn));

    JS_FreeCString(ctx, event_name);
    return JS_UNDEFINED;
}
```

---

## 8. BotClient — Headless client

### 8.1 Rakenne

BotClient jakaa verkko- ja kryptokerroksen oikean clientin kanssa,
mutta ei sisällä UI-kerrosta (notcurses), äänijärjestelmää (voice),
eikä link previewtä.

```
BotClient
├── NetClient          ← TLS/TCP, frame read/write (jaettu koodi)
├── CryptoEngine       ← Signal Protocol (jaettu koodi)
├── BotState           ← minimaalinen tila: kanavat, online status
└── PluginBridge       ← yhdistää BotClient ↔ PluginInstance
```

### 8.2 Jaetut komponentit CMakessa

```cmake
# Jaettu kirjasto: verkko + krypto
add_library(grotto-core STATIC
    src/net/net_client.cpp
    src/net/message_handler.cpp
    src/crypto/crypto_engine.cpp
    src/crypto/identity.cpp
    src/crypto/signal_store.cpp
    src/db/local_store.cpp
    src/config.cpp
)

# Oikea client — core + UI + voice
add_executable(grotto-client
    src/main.cpp
    src/app.cpp
    src/ui/...
    src/voice/...
)
target_link_libraries(grotto-client PRIVATE grotto-core notcurses ...)

# Bot host — core + QuickJS + plugin system
add_executable(grotto-bot-host
    src/bot/bot_main.cpp
    src/plugin/plugin_manager.cpp
    src/plugin/plugin_instance.cpp
    src/plugin/bot_plugin_instance.cpp
    src/plugin/bot_client.cpp
)
target_link_libraries(grotto-bot-host PRIVATE grotto-core quickjs ...)

# Server — oma binary + server_extension pluginit
add_executable(grotto-server
    src/server/main.cpp
    src/server/...
    src/plugin/plugin_manager.cpp
    src/plugin/plugin_instance.cpp
    src/plugin/server_ext_plugin.cpp
)
target_link_libraries(grotto-server PRIVATE quickjs ...)
```

### 8.3 Bot-hostin käynnistys

Bot-plugineille on erillinen prosessi (`grotto-bot-host`) joka:
1. Lukee config.toml:sta serverin osoitteen
2. Skannaa plugins/ -hakemiston
3. Luo BotClient per bot-plugin (oma identiteetti)
4. Yhdistää jokaisen botin serverille
5. Pyörittää Asio event loopia + QuickJS tick-timeria

```
$ grotto-bot-host --config bot-host.toml --plugins ./plugins/
[INFO] Loading plugin: dicebot (bot)
[INFO] Loading plugin: welcomebot (bot)
[INFO] DiceBot: connecting to grotto.example.com:6667...
[INFO] DiceBot: authenticated, joining #general, #games
[INFO] WelcomeBot: connecting to grotto.example.com:6667...
[INFO] WelcomeBot: authenticated, joining #general
[INFO] All plugins loaded. Running.
```

---

## 9. Slash-komennot — rekisteröinti ja dispatch

### 9.1 Kaksi tasoa

**Metadatarekisteröinti serverillä:**
Botti lähettää kirjautuessaan `MT_COMMAND_REGISTER` -viestin jossa
komentojen nimet ja kuvaukset. Server tallentaa nämä ja välittää ne
clienteille → tab-complete toimii kaikilla.

**Ajonaikainen käsittely botti-clientissä:**
Kun käyttäjä lähettää `/roll 2d6`, se menee kanavalle normaalina viestinä.
Botti vastaanottaa viestin, tunnistaa slash-komennon, ajaa handlerin ja
lähettää vastauksen.

### 9.2 Uusi Protobuf-viesti

```protobuf
message CommandRegister {
  repeated CommandDef commands = 1;
}

message CommandDef {
  string name        = 1;   // "/roll"
  string description = 2;   // "Roll NdN dice"
  string usage       = 3;   // "/roll [NdN]"
  string bot_user_id = 4;   // "DiceBot" — kuka omistaa
}

// Lisätään MessageType:en:
// MT_COMMAND_REGISTER = 60;
```

### 9.3 Flow

```
Käyttäjä kirjoittaa: /roll 2d6

Client-puoli:
  1. CommandParser tunnistaa /roll
  2. Tarkistaa: onko tämä lokaali komento? (client_extension)
     → Kyllä: aja lokaalisti, älä lähetä serverille
     → Ei: lähetä normaalina viestinä kanavalle

Server-puoli:
  3. Server vastaanottaa ChatEnvelope (E2E salattu)
  4. Server relay:aa kanavalle normaalisti (ei osaa avata)

Botti-puoli:
  5. DiceBot vastaanottaa viestin, purkaa salauksen
  6. PluginManager parsii "/roll 2d6" → PluginCommand
  7. Dispatch → dicebot:n JS callback
  8. JS: grotto.sendMessage(ctx.channel, "🎲 Sepi: 2d6 → 3 + 5 = 8")
```

---

## 10. Virheenkäsittely ja sandbox

### 10.1 JS-virheiden eristys

```cpp
bool PluginInstance::call_js_safe(JSValue fn, std::span<JSValue> args) {
    JSValue result = JS_Call(js_, fn, JS_UNDEFINED,
                             static_cast<int>(args.size()), args.data());

    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(js_);
        const char* str = JS_ToCString(js_, exc);

        spdlog::error("[PLUGIN:{}] JS exception: {}",
                      meta_.name, str ? str : "<unknown>");

        if (str) JS_FreeCString(js_, str);
        JS_FreeValue(js_, exc);
        JS_FreeValue(js_, result);

        error_count_++;
        if (error_count_ >= kMaxErrorsBeforeUnload) {
            spdlog::error("[PLUGIN:{}] Too many errors ({}), unloading",
                          meta_.name, error_count_);
            state_ = PluginState::Error;
            // PluginManager käsittelee unloadin seuraavassa tick:ssä
        }
        return false;
    }

    JS_FreeValue(js_, result);
    return true;
}
```

### 10.2 Resurssirajoitukset per plugin

| Resurssi                | Raja           | Perustelu                              |
|-------------------------|----------------|----------------------------------------|
| Muisti (JS heap)        | 16 MB          | Estää yhtä pluginia syömästä kaikkea   |
| Stack koko              | 1 MB           | Estää äärettömät rekursiot             |
| Max virheitä / tunti    | 50             | Automaattinen unload rikkoutuneelle    |
| HTTP timeout            | 10 s           | Estää ikuisesti roikkuvat pyynnöt      |
| Max HTTP pyynnöt / min  | 30             | Rate limit ulkoisille API-kutsuille    |
| Tiedoston max koko      | 1 MB           | Pluginin data/ -hakemiston tiedostot   |

### 10.3 Tilat

```cpp
enum class PluginState {
    Loading,     // init() käynnissä
    Running,     // normaali toiminta
    Error,       // liikaa virheitä, odottaa unloadia
    Stopped,     // shutdown() kutsuttu, odottaa poistoa
};
```

---

## 11. Hot-reload

Kehittäjäystävällinen: plugin voidaan ladata uudelleen ilman koko
prosessin uudelleenkäynnistystä.

```
reload("dicebot"):
  1. Kutsuu JS: grotto.onShutdown() — plugin voi siivota
  2. Vapauttaa JSRuntime (kaikki JS-tila häviää)
  3. Jos bot-plugin: BotClient pysyy yhdistettynä (ei reconnect)
  4. Parsii plugin.json uudelleen
  5. Luo uusi JSRuntime + JSContext
  6. Ajaa entry pointin uudelleen
  7. Plugin rekisteröi event handlerit ja komennot alusta
```

BotClientin yhteys serverille pidetään auki reload:ssa — vain JS-puoli
ladataan uudelleen. Näin botti ei näy offline-tilassa hetkeksikään.

---

## 12. Esimerkkiplugin: DiceBot

### plugin.json

```json
{
  "name": "dicebot",
  "display_name": "Dice Bot",
  "version": "1.0.0",
  "type": "bot",
  "entry": "main.js",
  "description": "Roll dice with /roll command",
  "min_host_version": "0.1.0",
  "bot": {
    "user_id": "DiceBot",
    "auto_join_channels": ["#general", "#games"]
  },
  "commands": [
    {
      "name": "/roll",
      "description": "Roll dice (e.g. /roll 2d6)",
      "usage": "/roll [NdN | N]"
    }
  ],
  "permissions": ["read_messages", "send_messages", "register_commands"]
}
```

### main.js

```js
// DiceBot — simple dice roller

function parseDice(input) {
    if (!input) return { count: 1, sides: 6 };

    const match = input.match(/^(\d+)d(\d+)$/i);
    if (!match) {
        const n = parseInt(input);
        if (!isNaN(n) && n >= 1 && n <= 100) return { count: 1, sides: n };
        return null;
    }

    const count = parseInt(match[1]);
    const sides = parseInt(match[2]);

    if (count < 1 || count > 20 || sides < 2 || sides > 1000) return null;
    return { count, sides };
}

function rollDice(count, sides) {
    const rolls = [];
    for (let i = 0; i < count; i++) {
        rolls.push(Math.floor(Math.random() * sides) + 1);
    }
    return rolls;
}

// Rekisteröi /roll komento
grotto.onCommand("/roll", {
    description: "Roll NdN dice",
    usage: "/roll [NdN]"
}, (ctx) => {
    const dice = parseDice(ctx.args[0]);
    if (!dice) {
        grotto.sendMessage(ctx.channel,
            `${ctx.sender}: Invalid format. Usage: /roll 2d6`);
        return;
    }

    const rolls = rollDice(dice.count, dice.sides);
    const total = rolls.reduce((a, b) => a + b, 0);

    if (dice.count === 1) {
        grotto.sendMessage(ctx.channel,
            `🎲 ${ctx.sender}: d${dice.sides} → ${total}`);
    } else {
        grotto.sendMessage(ctx.channel,
            `🎲 ${ctx.sender}: ${dice.count}d${dice.sides} → ${rolls.join(" + ")} = ${total}`);
    }
});

grotto.log.info("DiceBot loaded!");
```

---

## 13. Toteutusjärjestys

```
Vaihe 1 — Perusrakenne (3-4 pv)
  ✦ PluginMeta + plugin.json parsinta
  ✦ PluginInstance base: QuickJS runtime init/teardown
  ✦ PluginManager: discover(), load(), unload()
  ✦ Yksikkötestit: metadata parsinta, runtime elinkaari

Vaihe 2 — JS API (3-4 pv)
  ✦ grotto-globaalin setup: on(), onCommand(), log
  ✦ setTimeout / setInterval toteutus
  ✦ getConfig() — toml → JS-objekti
  ✦ Tick-integraatio Asio event loopiin
  ✦ Yksikkötestit: JS API kutsut, event dispatch

Vaihe 3 — BotClient (1 vko)
  ✦ grotto-core -kirjasto (NetClient + CryptoEngine erilleen)
  ✦ BotClient: headless client, connect + auth
  ✦ BotPluginInstance: sendMessage, joinChannel
  ✦ grotto-bot-host binary
  ✦ Integraatiotesti: botti yhdistää serverille, vastaanottaa viestin

Vaihe 4 — Slash-komennot (3-4 pv)
  ✦ MT_COMMAND_REGISTER protobuf-viesti
  ✦ Server: komentometadatan tallennus, välitys clienteille
  ✦ Client: tab-complete rekisteröidyille komennoille
  ✦ Botti: viestin parsinta → PluginCommand dispatch

Vaihe 5 — Client Extension (3-4 pv)
  ✦ ClientExtPlugin: integraatio AppStateen ja NetClientiin
  ✦ Lokaalit slash-komennot
  ✦ grotto-client binary: plugin-lataus käynnistyksessä

Vaihe 6 — Server Extension (2-3 pv)
  ✦ ServerExtPlugin: integraatio ChannelManageriin ja SessionManageriin
  ✦ Metadata-eventit: join/part, presence, auth
  ✦ Server: plugin-lataus käynnistyksessä

Vaihe 7 — Polish ja turvallisuus (3-4 pv)
  ✦ Hot-reload
  ✦ Permission-tarkistukset
  ✦ Resurssirajoitukset (muisti, stack, error count)
  ✦ HTTP-client plugineille (libcurl async)
  ✦ fs.readFile / fs.writeFile sandboxattu pluginin hakemistoon
  ✦ Dokumentaatio: plugin-kehittäjän opas
```

**Arvioitu kokonaisaika: 4-5 viikkoa**

---

## 14. Avoimet kysymykset

1. **Botin Ed25519-avainten hallinta:** Generoidaanko automaattisesti
   ensimmäisellä käynnistyksellä? Tallennetaanko erilliseen tiedostoon
   per plugin vai yhteiseen keystoreen?

2. **Plugin-jakelu:** Tarvitaanko keskitetty plugin-repositorio vai
   riittääkö manuaalinen kopiointi plugins/ -hakemistoon?

3. **Plugin-päivitykset:** Hot-reload riittää kehitykseen, mutta
   tuotantokäytössä tarvitaanko versionhallinta + automaattipäivitys?

4. **Client_extension UI-laajennus:** Kuinka paljon clientin UI:ta
   plugin saa muokata? Pelkät notifikaatiot vai myös custom-paneelit?

5. **Plugin välinen kommunikaatio:** Pitääkö pluginien voida puhua
   toisilleen? Esim. botti A kutsuu botti B:n funktiota.
