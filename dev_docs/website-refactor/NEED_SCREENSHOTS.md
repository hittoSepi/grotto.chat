Kuvat joista haluan placeholderit:

Server arkkitehtuuri — clients ↔ TLS ↔ server relay ↔ E2E encrypted, server never sees plaintext
Network topologia — internet ↔ firewall ↔ reverse proxy ↔ grotto-server ↔ database
Desktop TUI screenshot — kanavalista, viestialue, käyttäjälista, input bar
Android app screenshots (3-4 kpl) — setup screen, chat view, voice room, settings
Plugin arkkitehtuuri — PluginManager ↔ PluginInstance (Bot/ClientExt/ServerExt) ↔ QuickJS runtime
Plugin event flow — käyttäjä lähettää /roll → server relay → bot vastaanottaa → JS callback → vastaus