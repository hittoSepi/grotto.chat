# Grotto Infrastructure

Deployable Grotto public-facing infrastructure package:
- **Directory Service**: Public server listing API (Node.js)
- **Landing Page**: Static marketing and server list site
- **TURN Service**: Coturn-based ICE relay for desktop voice
- **Nginx**: Reverse proxy and TLS termination for production

## Quick Start

### Prerequisites

- Docker and Docker Compose
- DNS records for the directory and landing domains
- TLS certificates for production, or let `deploy.sh` obtain them

### Development

```bash
docker-compose up -d
docker-compose logs -f
docker-compose down
```

To run only the directory service:

```bash
docker compose -f directory-docker-compose.yml up -d --build
docker compose -f directory-docker-compose.yml logs -f
docker compose -f directory-docker-compose.yml down
```

Services:
- Directory API: `http://localhost:3000`
- Landing Page: `http://localhost:8080`
- TURN: `3478/tcp+udp`, `5349/tcp+udp`, relay UDP `49160-49200`

The published ports are bound to `127.0.0.1`, so they stay available for a host-level nginx reverse proxy without exposing the containers directly.

### Host nginx reverse proxy

If the server already runs nginx for TLS, do not start the Docker `nginx` service. Start only:

```bash
docker compose up -d directory landing turn
```

Then point the host nginx config at:
- `127.0.0.1:8080` for the landing page
- `127.0.0.1:3000` for the directory API

An example config is available at `nginx/host-proxy.conf.example`.
TURN stays outside nginx and listens directly on `3478` / `5349`. This deployment does not use port `443` for TURN.
If landing and directory use different domains, the reverse proxy for the directory domain must return `Access-Control-Allow-Origin` for `/api/` responses.

### Production

```bash
./deploy.sh
```

The installer asks for:
- Directory API domain (e.g., `directory.example.com`)
- Landing page domain (e.g., `chat.example.com`)
- Optional TURN domain and static TURN credentials
- TLS mode and optional Let's Encrypt validation method

`deploy.sh` is for the bundled Docker nginx setup that terminates TLS itself. If you already have nginx on the host and it owns ports `80/443`, use the host nginx flow above instead of the Docker `nginx` service.

## Services

### Directory Service (`grotto-directory/`)

Node.js API for managing public Grotto server listings.

**Endpoints:**
- `POST /api/servers/register` — Register a new server
- `POST /api/servers/ping` — Keep server on the list (5 min interval)
- `POST /api/servers/unregister` — Remove server from list
- `GET /api/servers` — List all public servers
- `GET /api/servers/:id` — Get single server details
- `GET /api/health` — Health check

**Environment variables:**
- `PORT` - server port, default `3000`
- `NODE_ENV` - environment name
- `DATA_DIR` - data directory for persistent storage

**Server timeout:** Servers are removed from the list if they don't ping within 10 minutes.

### Landing Page (`grotto-landing/`)

Static landing site with:
- Public server list fetched from directory API
- Quick connect via `ircord://` protocol
- Manual fallback when the native client is missing
- Download links for clients

`deploy.sh` generates `grotto-landing/config.js` with the correct directory and landing URLs for the target environment.

### TURN Service (`turn/`)

Coturn-based STUN/TURN relay for desktop WebRTC voice.

Default ports:
- `3478/tcp`
- `3478/udp`
- `5349/tcp`
- `5349/udp`
- relay UDP range `49160-49200`

`deploy.sh` generates `turn/turnserver.conf` with the selected domain, credentials, relay ports, and certificate paths.

## Structure

```
grotto-infra/
├── docker-compose.yml
├── deploy.sh
├── nginx/
│   ├── nginx.conf           # Production reverse proxy + SSL
│   └── static-site.conf     # Local development static file serving
├── grotto-directory/
│   ├── Dockerfile
│   ├── package.json
│   ├── server.js            # Directory API with persistent storage
│   └── README.md
└── grotto-landing/
    ├── index.html
    ├── style.css
    ├── servers.js           # Server list fetching
    └── README.md
```

## Deployment

### 1. Clone

```bash
git clone <repo-url> grotto-infra
cd grotto-infra
```

### 2. Run installer

```bash
sudo ./deploy.sh
```

The installer will:
- Prompt for domain names
- Prompt for optional TURN/ICE settings
- Obtain SSL certificates (Let's Encrypt or self-signed)
- Generate runtime configuration
- Start Docker containers

### 3. Verify

```bash
curl https://directory.example.com/api/health
curl https://chat.example.com
turnutils_uclient -T -n 1 -u grotto -w 'your-turn-password' turn.example.com
```

### 4. DNS

Point both domains to the server IP:
- `directory.example.com` → Server IP
- `chat.example.com` → Server IP

If TURN is enabled, also point the TURN domain to the same server IP:
- `turn.example.com` -> Server IP

## Maintenance

### Update services

```bash
sudo grotto-infra-update
```

The update command refreshes `/opt/grotto-infra` from git, reinstalls the helper itself, and rebuilds the active Docker services. It starts `turn` only when `turn/turnserver.conf` exists.

### View logs

```bash
docker-compose logs -f
docker-compose logs -f directory
docker-compose logs -f landing
docker-compose logs -f turn
docker-compose logs -f nginx
```

### Backup

```bash
# Backup directory data
docker run --rm -v grotto-infra_directory-data:/data -v $(pwd):/backup alpine tar czf /backup/directory-backup.tar.gz -C /data .
```

### Restore

```bash
# Restore directory data
docker run --rm -v grotto-infra_directory-data:/data -v $(pwd):/backup alpine tar xzf /backup/directory-backup.tar.gz -C /data
```

## Local Development Without Docker

### Directory

```bash
cd grotto-directory
npm install
npm run dev
```

### Landing

```bash
cd grotto-landing
python3 -m http.server 8080
```

For local landing development, `servers.js` falls back to `http://localhost:3000` if `config.js` is not present. On non-localhost deployments it falls back to the current site origin, which lets a host nginx config proxy `/api/` on the same domain.

## Grotto Protocol Handler

The landing page uses the `ircord://` protocol to launch the native client:

```
ircord://host:port
```

Example: `ircord://chat.example.com:6697`

If the protocol handler is not installed, the landing page shows manual connection instructions.

## Related Projects

- [grotto-server](../grotto-server) — Grotto server software
- [grotto-client](../grotto-client) — Desktop client
- [grotto-android](../grotto-android) — Android client
