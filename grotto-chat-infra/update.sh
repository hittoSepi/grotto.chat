#!/bin/bash
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[ OK ]${NC}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[FAIL]${NC}  $*"; exit 1; }

INSTALL_DIR="${GROTTO_CHAT_INFRA_INSTALL_DIR:-/opt/grotto-infra}"
DC=""

detect_compose() {
    if docker compose version >/dev/null 2>&1; then
        DC="docker compose"
    elif command -v docker-compose >/dev/null 2>&1; then
        DC="docker-compose"
    else
        error "Docker Compose is not installed"
    fi
}

collect_services() {
    SERVICES=(directory landing)
    if [ -s "$INSTALL_DIR/turn/turnserver.conf" ]; then
        SERVICES+=(turn)
    fi
}

[ "$(id -u)" -eq 0 ] || error "Run with sudo: sudo grotto-infra-update"
[ -d "$INSTALL_DIR/.git" ] || error "Install directory not found: $INSTALL_DIR"

detect_compose
collect_services

cd "$INSTALL_DIR"

info "Updating Grotto infrastructure in $INSTALL_DIR"
git fetch --tags origin
git pull --ff-only

install -m 755 "$INSTALL_DIR/update.sh" /usr/local/bin/grotto-infra-update
ok "Update command refreshed"

info "Rebuilding services: ${SERVICES[*]}"
$DC up -d --build "${SERVICES[@]}"

sleep 3
if curl -sf http://localhost:3000/api/health >/dev/null 2>&1; then
    ok "Directory service is healthy"
else
    warn "Directory health check failed; review logs with: cd $INSTALL_DIR && $DC logs -f"
fi

ok "Grotto infrastructure update complete"
