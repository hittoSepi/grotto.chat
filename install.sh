#!/usr/bin/env bash
set -euo pipefail

# Grotto installer
# Usage: curl -fsSL https://file.rausku.com/install.sh | bash
#   or:  curl -fsSL https://file.rausku.com/install.sh | bash -s -- --server

BASE_URL="https://file.rausku.com"
VERSION="latest"
INSTALL_DIR="${INSTALL_DIR:-$HOME/.local/bin}"
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/grotto"

# ── Colors ──────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

info()  { printf "${CYAN}[*]${NC} %s\n" "$*"; }
ok()    { printf "${GREEN}[+]${NC} %s\n" "$*"; }
warn()  { printf "${YELLOW}[!]${NC} %s\n" "$*"; }
err()   { printf "${RED}[-]${NC} %s\n" "$*" >&2; }
die()   { err "$*"; exit 1; }

# ── Parse args ──────────────────────────────────────────────────────────
COMPONENT="client"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --server)  COMPONENT="server" ;;
        --client)  COMPONENT="client" ;;
        --dir)     INSTALL_DIR="$2"; shift ;;
        --help|-h)
            cat <<'HELP'
Grotto installer

Usage:
  curl -fsSL https://file.rausku.com/install.sh | bash            # install client
  curl -fsSL https://file.rausku.com/install.sh | bash -s -- --server  # install server

Options:
  --client        Install the TUI client (default)
  --server        Install the relay server
  --dir <path>    Install directory (default: ~/.local/bin)
  -h, --help      Show this help
HELP
            exit 0
            ;;
        *) die "Unknown option: $1" ;;
    esac
    shift
done

# ── Detect platform ────────────────────────────────────────────────────
detect_platform() {
    local os arch

    case "$(uname -s)" in
        Linux*)  os="linux" ;;
        Darwin*) os="macos" ;;
        MINGW*|MSYS*|CYGWIN*) os="windows" ;;
        *) die "Unsupported OS: $(uname -s)" ;;
    esac

    case "$(uname -m)" in
        x86_64|amd64)  arch="x64" ;;
        aarch64|arm64) arch="arm64" ;;
        *) die "Unsupported architecture: $(uname -m)" ;;
    esac

    echo "${os}-${arch}"
}

PLATFORM="$(detect_platform)"
info "Detected platform: ${BOLD}${PLATFORM}${NC}"
info "Installing: ${BOLD}grotto-${COMPONENT}${NC}"

# ── Check dependencies ─────────────────────────────────────────────────
for cmd in curl tar; do
    command -v "$cmd" >/dev/null 2>&1 || die "Required command not found: $cmd"
done

# ── Download & extract ──────────────────────────────────────────────────
ARCHIVE="grotto-${COMPONENT}-${PLATFORM}.tar.gz"
DOWNLOAD_URL="${BASE_URL}/${VERSION}/${ARCHIVE}"

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

info "Downloading ${ARCHIVE}..."
HTTP_CODE=$(curl -fsSL -w '%{http_code}' -o "${TMPDIR}/${ARCHIVE}" "${DOWNLOAD_URL}" 2>/dev/null) || true

if [[ ! -f "${TMPDIR}/${ARCHIVE}" ]] || [[ "${HTTP_CODE}" != "200" ]]; then
    die "Download failed (HTTP ${HTTP_CODE:-???}). Check ${DOWNLOAD_URL}"
fi

ok "Downloaded $(du -h "${TMPDIR}/${ARCHIVE}" | cut -f1)"

info "Extracting to ${INSTALL_DIR}..."
mkdir -p "${INSTALL_DIR}"
tar -xzf "${TMPDIR}/${ARCHIVE}" -C "${TMPDIR}"

# Find and install binaries
BINARY_NAME="grotto-${COMPONENT}"
find "${TMPDIR}" -type f \( -name "${BINARY_NAME}" -o -name "${BINARY_NAME}.exe" -o -name "*.so" -o -name "*.dll" \) | while read -r f; do
    install -m 755 "$f" "${INSTALL_DIR}/$(basename "$f")"
done

ok "Installed to ${INSTALL_DIR}"

# ── Create default config (client only) ─────────────────────────────────
if [[ "${COMPONENT}" == "client" ]]; then
    if [[ ! -f "${CONFIG_DIR}/client.toml" ]]; then
        mkdir -p "${CONFIG_DIR}"
        cat > "${CONFIG_DIR}/client.toml" <<'TOML'
[server]
host = ""       # Server hostname or IP
port = 6667

[identity]
user_id = ""    # Your username (prompted on first run if empty)

[ui]
theme = "tokyo-night"
timestamp_format = "%H:%M"
max_messages = 1000

[voice]
input_device  = ""
output_device = ""
opus_bitrate  = 64000
frame_ms      = 20

[tls]
verify_peer = true
TOML
        ok "Created config: ${CONFIG_DIR}/client.toml"
        warn "Edit ${CONFIG_DIR}/client.toml and set your server address before running"
    else
        warn "Config already exists: ${CONFIG_DIR}/client.toml (not overwritten)"
    fi
fi

# ── Check PATH ──────────────────────────────────────────────────────────
if [[ ":${PATH}:" != *":${INSTALL_DIR}:"* ]]; then
    warn "${INSTALL_DIR} is not in your PATH"
    echo ""
    echo "  Add it to your shell profile:"
    echo ""
    echo "    echo 'export PATH=\"${INSTALL_DIR}:\$PATH\"' >> ~/.bashrc"
    echo ""
fi

# ── Done ────────────────────────────────────────────────────────────────
echo ""
printf "${GREEN}${BOLD}"
cat <<'BANNER'
  ___ ____   ____              _
 |_ _|  _ \ / ___|___  _ __ __| |
  | || |_) | |   / _ \| '__/ _` |
  | ||  _ <| |__| (_) | | | (_| |
 |___|_| \_\\____\___/|_|  \__,_|

BANNER
printf "${NC}"
ok "Installation complete!"
echo ""
if [[ "${COMPONENT}" == "client" ]]; then
    info "Run:  ${BOLD}grotto-client${NC}"
    info "Config: ${CONFIG_DIR}/client.toml"
else
    info "Run:  ${BOLD}grotto-server${NC}"
    info "See:  ${BOLD}grotto-server --help${NC} for options"
fi
echo ""
