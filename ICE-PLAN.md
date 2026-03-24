# ICE-PLAN

## Summary

- Add a coturn-based ICE/TURN service to both `grotto-infra` and the native `grotto-server` installer.
- Fix desktop client voice config plumbing so `ice_servers`, `turn_username`, and `turn_password` are fully loaded, saved, exported, and imported.
- Keep scope on desktop client plus deployment/install paths. Android remains out of scope for this phase.
- Use static TURN credentials. No Grotto protocol changes and no runtime TURN credential API.

## Key Changes

- Desktop client:
  - `[voice]` supports `ice_servers`, `turn_username`, `turn_password`
  - Example config and README document Google STUN fallback versus self-hosted TURN
  - No Settings UI changes in this phase
- `grotto-infra`:
  - Add coturn service, config generation, firewall/docs updates, and TURN-related deploy variables
  - Use ports `3478/tcp+udp`, `5349/tcp+udp`, and relay UDP range `49160-49200`
- `grotto-server/scripts/install.sh`:
  - Optional coturn installation and `/etc/turnserver.conf` generation
  - Reuse Grotto TLS certs by default
  - Open firewall ports and restart coturn on certificate renewal

## Public Config

```toml
[voice]
ice_servers = [
  "stun:turn.example.com:3478",
  "turn:turn.example.com:3478?transport=udp",
  "turns:turn.example.com:5349?transport=tcp",
]
turn_username = "grotto"
turn_password = "replace-with-your-turn-password"
```

- Empty `ice_servers` keeps the built-in Google STUN fallback.
- If `ice_servers` is defined, the client uses only that list.
- If `turn_username` is set, the same credentials are applied to all configured ICE servers.

## Validation

- Desktop config round-trip and export/import tests cover ICE/TURN fields.
- `docker compose config` validates the infra stack with the new TURN service.
- `bash -n grotto-server/scripts/install.sh` validates the native installer shell syntax.
- Manual acceptance: two desktop clients can establish voice through TURN when direct P2P is insufficient.
