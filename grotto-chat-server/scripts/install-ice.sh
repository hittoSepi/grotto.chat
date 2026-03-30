#!/usr/bin/env bash
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

info() { echo -e "${CYAN}[INFO]${NC} $*"; }
ok() { echo -e "${GREEN}[ OK ]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
fail() { echo -e "${RED}[FAIL]${NC} $*" >&2; exit 1; }

usage() {
    cat <<'EOF'
Usage:
  sudo ./install-ice.sh --domain turn.example.com [options]

Required:
  --domain <fqdn>          TURN domain, used for TLS cert lookup and client config

Optional:
  --realm <realm>          TURN realm (default: same as --domain)
  --user <username>        Static TURN username (default: grotto)
  --password <password>    Static TURN password (default: generated)
  --external-ip <ip>       Public IP to advertise to peers (default: autodetect)
  --cert <path>            TLS certificate path
  --key <path>             TLS private key path
  --skip-ufw               Do not touch UFW rules
  --print-only             Print generated config and client snippet, do not install
  -h, --help               Show this help

Example:
  sudo ./install-ice.sh --domain cave.grotto.chat
EOF
}

require_root() {
    if [ "$(id -u)" -ne 0 ]; then
        fail "Run as root: sudo $0 --domain <fqdn>"
    fi
}

detect_external_ip() {
    local ip
    ip="$(curl -fsS --max-time 5 https://api.ipify.org 2>/dev/null || true)"
    if [ -z "$ip" ]; then
        ip="$(curl -fsS --max-time 5 https://ifconfig.me 2>/dev/null || true)"
    fi
    printf '%s' "$ip"
}

generate_password() {
    if command -v openssl >/dev/null 2>&1; then
        openssl rand -base64 24 | tr -d '\n'
        return
    fi
    tr -dc 'A-Za-z0-9' </dev/urandom | head -c 32
}

DOMAIN=""
REALM=""
TURN_USER="grotto"
TURN_PASSWORD=""
EXTERNAL_IP=""
CERT_PATH=""
KEY_PATH=""
SKIP_UFW=0
PRINT_ONLY=0

while [ "$#" -gt 0 ]; do
    case "$1" in
        --domain)
            DOMAIN="${2:-}"
            shift 2
            ;;
        --realm)
            REALM="${2:-}"
            shift 2
            ;;
        --user)
            TURN_USER="${2:-}"
            shift 2
            ;;
        --password)
            TURN_PASSWORD="${2:-}"
            shift 2
            ;;
        --external-ip)
            EXTERNAL_IP="${2:-}"
            shift 2
            ;;
        --cert)
            CERT_PATH="${2:-}"
            shift 2
            ;;
        --key)
            KEY_PATH="${2:-}"
            shift 2
            ;;
        --skip-ufw)
            SKIP_UFW=1
            shift
            ;;
        --print-only)
            PRINT_ONLY=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "Unknown option: $1"
            ;;
    esac
done

[ -n "$DOMAIN" ] || { usage; fail "--domain is required"; }
[ -n "$REALM" ] || REALM="$DOMAIN"
[ -n "$TURN_PASSWORD" ] || TURN_PASSWORD="$(generate_password)"
[ -n "$CERT_PATH" ] || CERT_PATH="/etc/letsencrypt/live/${DOMAIN}/fullchain.pem"
[ -n "$KEY_PATH" ] || KEY_PATH="/etc/letsencrypt/live/${DOMAIN}/privkey.pem"
[ -n "$EXTERNAL_IP" ] || EXTERNAL_IP="$(detect_external_ip)"

TURN_CONFIG=$(cat <<EOF
fingerprint
lt-cred-mech
realm=${REALM}
user=${TURN_USER}:${TURN_PASSWORD}
listening-ip=0.0.0.0
${EXTERNAL_IP:+external-ip=${EXTERNAL_IP}}
listening-port=3478
tls-listening-port=5349
min-port=49160
max-port=49200
cert=${CERT_PATH}
pkey=${KEY_PATH}
no-cli
no-multicast-peers
no-tlsv1
no-tlsv1_1
EOF
)

CLIENT_SNIPPET=$(cat <<EOF
[voice]
ice_servers = [
  "stun:${DOMAIN}:3478",
  "turn:${DOMAIN}:3478?transport=udp",
  "turns:${DOMAIN}:5349?transport=tcp",
]
turn_username = "${TURN_USER}"
turn_password = "${TURN_PASSWORD}"
EOF
)

SERVER_SNIPPET=$(cat <<EOF
[voice]
ice_servers = [
  "stun:${DOMAIN}:3478",
  "turn:${DOMAIN}:3478?transport=udp",
  "turns:${DOMAIN}:5349?transport=tcp",
]
turn_username = "${TURN_USER}"
turn_password = "${TURN_PASSWORD}"
EOF
)

if [ "$PRINT_ONLY" -eq 1 ]; then
    printf '%s\n\n' "$TURN_CONFIG"
    printf '%s\n\n' "$SERVER_SNIPPET"
    printf '%s\n' "$CLIENT_SNIPPET"
    exit 0
fi

require_root

if [ ! -f "$CERT_PATH" ]; then
    fail "Certificate not found: $CERT_PATH"
fi

if [ ! -f "$KEY_PATH" ]; then
    fail "Private key not found: $KEY_PATH"
fi

info "Installing coturn"
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq coturn curl
ok "coturn installed"

info "Writing /etc/turnserver.conf"
cp /etc/turnserver.conf "/etc/turnserver.conf.bak.$(date +%s)" 2>/dev/null || true
printf '%s\n' "$TURN_CONFIG" > /etc/turnserver.conf
printf 'TURNSERVER_ENABLED=1\n' > /etc/default/coturn
ok "TURN config written"

if [ "$SKIP_UFW" -eq 0 ] && command -v ufw >/dev/null 2>&1; then
    info "Opening TURN ports in UFW"
    ufw allow 3478/tcp comment "TURN TCP" >/dev/null 2>&1 || true
    ufw allow 3478/udp comment "TURN UDP" >/dev/null 2>&1 || true
    ufw allow 5349/tcp comment "TURNS TCP" >/dev/null 2>&1 || true
    ufw allow 5349/udp comment "TURNS UDP" >/dev/null 2>&1 || true
    ufw allow 49160:49200/udp comment "TURN relay UDP" >/dev/null 2>&1 || true
    ok "UFW updated"
fi

info "Enabling and restarting coturn"
systemctl enable coturn >/dev/null 2>&1 || true
systemctl restart coturn
sleep 2

if [ "$(systemctl is-active coturn 2>/dev/null || true)" != "active" ]; then
    journalctl -u coturn -n 30 --no-pager || true
    fail "coturn failed to start"
fi
ok "coturn is running"

if ss -tuln | grep -qE ':(3478|5349)\s'; then
    ok "TURN ports are listening"
else
    warn "TURN ports do not appear to be listening yet"
fi

echo
ok "Install complete"
echo
echo "TURN config:"
echo "  domain:       ${DOMAIN}"
echo "  realm:        ${REALM}"
echo "  external_ip:  ${EXTERNAL_IP:-<not detected>}"
echo "  username:     ${TURN_USER}"
echo "  password:     ${TURN_PASSWORD}"
echo
echo "Add this to client.toml:"
printf '%s\n' "$CLIENT_SNIPPET"
echo
echo "Add this to server.toml for auth-time ICE bootstrap:"
printf '%s\n' "$SERVER_SNIPPET"
echo
echo "Useful checks:"
echo "  systemctl status coturn"
echo "  journalctl -u coturn -n 50 --no-pager"
echo "  ss -tuln | grep -E '3478|5349|49160'"
