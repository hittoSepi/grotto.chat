/**
 * Grotto Landing Page - Server List
 */

const DEFAULT_CONFIG = {
    DIRECTORY_URL: getDefaultDirectoryUrl()
};

const RUNTIME_CONFIG = window.GROTTO_CHAT_CONFIG || {};
const DIRECTORY_URL = normalizeBaseUrl(RUNTIME_CONFIG.DIRECTORY_URL || DEFAULT_CONFIG.DIRECTORY_URL);

function getDefaultDirectoryUrl() {
    if (typeof window === 'undefined' || !window.location) {
        return 'http://localhost:3000';
    }

    const host = window.location.hostname;
    if (host === 'localhost' || host === '127.0.0.1') {
        return 'http://localhost:3000';
    }

    return window.location.origin;
}

function normalizeBaseUrl(url) {
    return String(url || '').replace(/\/+$/, '');
}

async function fetchServers() {
    const loadingEl = document.getElementById('servers-loading');
    const errorEl = document.getElementById('servers-error');
    const listEl = document.getElementById('servers-list');

    loadingEl.hidden = false;
    errorEl.hidden = true;
    listEl.hidden = true;

    try {
        const response = await fetch(`${DIRECTORY_URL}/api/servers`);
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }

        const data = await response.json();
        const servers = Array.isArray(data.servers) ? data.servers : [];

        loadingEl.hidden = true;

        if (servers.length > 0) {
            renderServers(servers);
            listEl.hidden = false;
            return;
        }

        listEl.innerHTML = `
            <article class="card server-card--live empty">
                <h3>No public servers available right now</h3>
                <p>The directory is online, but nobody has published a server yet. Host one and be the first.</p>
                <div class="server-actions">
                    <a class="btn btn--primary" href="docs/infrastructure.html">Read hosting guide</a>
                </div>
            </article>
        `;
        listEl.hidden = false;
    } catch (error) {
        console.error('Failed to fetch servers:', error);
        loadingEl.hidden = true;
        errorEl.hidden = false;
    }
}

function renderServers(servers) {
    const listEl = document.getElementById('servers-list');

    listEl.innerHTML = servers.map((server) => {
        const online = Boolean(server.online);
        const statusClass = online ? 'online' : 'offline';
        const statusText = online ? 'Online' : 'Offline';
        const connectUrl = `ircord://${server.host}:${server.port}`;
        const inviteUrl = buildInviteUrl(server);

        return `
            <article class="card server-card--live ${statusClass}">
                <div class="server-card__header">
                    <div>
                        <h3>${escapeHtml(server.name || 'Grotto Server')}</h3>
                        <p class="server-card__description">${escapeHtml(server.description || 'Public Grotto server')}</p>
                    </div>
                    <span class="server-card__status ${statusClass}">${statusText}</span>
                </div>

                <div class="server-card__meta-row">
                    <code class="server-address">${escapeHtml(server.host)}:${server.port}</code>
                    <span class="server-tag">public</span>
                    <span class="server-tag">${online ? 'joinable now' : 'directory entry'}</span>
                </div>

                <div class="server-actions">
                    <a href="${connectUrl}" class="btn btn--primary btn-connect" data-host="${escapeHtml(server.host)}" data-port="${server.port}">Connect</a>
                    <a href="${inviteUrl}" class="btn btn--ghost">Invite</a>
                    <button class="btn btn--ghost btn-copy" data-address="${escapeHtml(server.host)}:${server.port}" type="button">Copy</button>
                </div>
            </article>
        `;
    }).join('');

    document.querySelectorAll('.btn-copy').forEach((button) => {
        button.addEventListener('click', handleCopy);
    });

    document.querySelectorAll('.btn-connect').forEach((button) => {
        button.addEventListener('click', handleConnect);
    });
}

function buildInviteUrl(server) {
    const params = new URLSearchParams({
        server: server.id || server.name || server.host,
        host: server.host,
        port: String(server.port),
        name: server.name || 'Grotto Server',
        description: server.description || ''
    });

    return `invite.html?${params.toString()}`;
}

function handleCopy(event) {
    const button = event.currentTarget;
    const address = button.dataset.address || '';
    navigator.clipboard.writeText(address).then(() => {
        const original = button.textContent;
        button.textContent = 'Copied';
        setTimeout(() => {
            button.textContent = original;
        }, 1800);
    });
}

function handleConnect(event) {
    const button = event.currentTarget;
    const host = button.dataset.host;
    const port = button.dataset.port;

    if (!host || !port) {
        return;
    }

    window.location.href = `ircord://${host}:${port}`;

    setTimeout(() => {
        if (!document.hidden) {
            showProtocolHelp(host, port);
        }
    }, 500);

    event.preventDefault();
}

function showProtocolHelp(host, port) {
    closeModal();

    const serverAddress = host && port ? `${host}:${port}` : 'host:port';
    const helpHtml = `
        <div class="modal-overlay" id="protocol-modal">
            <div class="modal">
                <h3>Open Grotto client</h3>
                <p>The <code>ircord://</code> protocol handler did not open. You can still download the client or connect manually.</p>
                <div class="modal-options">
                    <div class="modal-option">
                        <h4>Download Grotto</h4>
                        <p>Install the native client for the cleanest join flow.</p>
                        <a href="#download" class="btn btn--primary" onclick="closeModal()">Go to downloads</a>
                    </div>
                    <div class="modal-option">
                        <h4>Connect manually</h4>
                        <p>Use this address in your Grotto client:</p>
                        <code class="address-box">${escapeHtml(serverAddress)}</code>
                        <button class="btn btn--ghost" type="button" onclick="copyManualAddress(this)">Copy address</button>
                    </div>
                </div>
                <button class="modal-close" type="button" aria-label="Close" onclick="closeModal()">&times;</button>
            </div>
        </div>
    `;

    document.body.insertAdjacentHTML('beforeend', helpHtml);
}

function closeModal() {
    const modal = document.getElementById('protocol-modal');
    if (modal) {
        modal.remove();
    }
}

function copyManualAddress(button) {
    const addressBox = button.previousElementSibling;
    const address = addressBox ? addressBox.textContent : '';
    navigator.clipboard.writeText(address).then(() => {
        const original = button.textContent;
        button.textContent = 'Copied';
        setTimeout(() => {
            button.textContent = original;
        }, 1800);
    });
}

function handleQuickConnect() {
    const addressInput = document.getElementById('server-address');
    const connectBtn = document.getElementById('connect-btn');

    if (!addressInput || !connectBtn) {
        return;
    }

    const submit = () => {
        const address = addressInput.value.trim();
        if (!address) {
            return;
        }

        let host = address;
        let port = 6697;

        if (address.includes(':')) {
            const [parsedHost, parsedPort] = address.split(':');
            host = parsedHost;
            port = Number.parseInt(parsedPort, 10);
        }

        if (!host || Number.isNaN(port)) {
            window.alert('Invalid address format. Use host:port or just host.');
            return;
        }

        window.location.href = `ircord://${host}:${port}`;

        setTimeout(() => {
            if (!document.hidden) {
                showProtocolHelp(host, port);
            }
        }, 500);
    };

    connectBtn.addEventListener('click', submit);
    addressInput.addEventListener('keypress', (event) => {
        if (event.key === 'Enter') {
            submit();
        }
    });
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = String(text || '');
    return div.innerHTML;
}

function parseUrlParams() {
    const hash = window.location.hash;
    if (!hash.startsWith('#connect=')) {
        return;
    }

    const address = decodeURIComponent(hash.substring(9));
    const addressInput = document.getElementById('server-address');
    if (addressInput) {
        addressInput.value = address;
    }
}

document.addEventListener('DOMContentLoaded', () => {
    fetchServers();
    handleQuickConnect();
    parseUrlParams();
    setInterval(fetchServers, 60000);
});

window.closeModal = closeModal;
window.copyManualAddress = copyManualAddress;
