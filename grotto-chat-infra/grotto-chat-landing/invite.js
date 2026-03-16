const INVITE_DEFAULTS = {
    DIRECTORY_URL: getInviteDefaultDirectoryUrl(),
    server: {
        id: 'public-demo',
        name: 'Public Grotto Server',
        description: 'A shared invite page for joining one public Grotto community.',
        host: 'chat.example.com',
        port: 6697,
        online: true,
        region: 'EU West',
        owner: 'Server admin'
    }
};

const INVITE_RUNTIME_CONFIG = window.GROTTO_CHAT_CONFIG || {};
const INVITE_DIRECTORY_URL = normalizeInviteBaseUrl(
    INVITE_RUNTIME_CONFIG.DIRECTORY_URL || INVITE_DEFAULTS.DIRECTORY_URL
);

function getInviteDefaultDirectoryUrl() {
    if (typeof window === 'undefined' || !window.location) {
        return 'http://localhost:3000';
    }

    const host = window.location.hostname;
    if (host === 'localhost' || host === '127.0.0.1') {
        return 'http://localhost:3000';
    }

    return window.location.origin;
}

function normalizeInviteBaseUrl(url) {
    return String(url || '').replace(/\/+$/, '');
}

function getInviteParams() {
    const params = new URLSearchParams(window.location.search);
    return {
        server: params.get('server') || '',
        host: params.get('host') || '',
        port: params.get('port') || '',
        name: params.get('name') || '',
        description: params.get('description') || '',
        token: params.get('token') || ''
    };
}

function matchServer(server, params) {
    if (params.server) {
        const key = params.server.toLowerCase();
        if (String(server.id || '').toLowerCase() === key) {
            return true;
        }
        if (String(server.slug || '').toLowerCase() === key) {
            return true;
        }
        if (String(server.name || '').toLowerCase() === key) {
            return true;
        }
    }

    if (params.host) {
        const serverHost = String(server.host || '').toLowerCase();
        const serverPort = String(server.port || '');
        if (serverHost === params.host.toLowerCase()) {
            return !params.port || serverPort === String(params.port);
        }
    }

    return false;
}

async function resolveInviteServer() {
    const params = getInviteParams();

    if (params.token) {
        return {
            ...INVITE_DEFAULTS.server,
            name: 'Private invite placeholder',
            description: 'Token-based private invites are reserved for a later backend implementation.',
            online: false
        };
    }

    const fallback = {
        ...INVITE_DEFAULTS.server,
        name: params.name || INVITE_DEFAULTS.server.name,
        description: params.description || INVITE_DEFAULTS.server.description,
        host: params.host || INVITE_DEFAULTS.server.host,
        port: Number.parseInt(params.port, 10) || INVITE_DEFAULTS.server.port
    };

    if (params.host || params.name || params.description) {
        return fallback;
    }

    try {
        const response = await fetch(`${INVITE_DIRECTORY_URL}/api/servers`);
        if (!response.ok) {
            throw new Error(`Directory request failed: ${response.status}`);
        }

        const data = await response.json();
        const servers = Array.isArray(data.servers) ? data.servers : [];
        const matched = servers.find((server) => matchServer(server, params));

        if (matched) {
            return {
                ...INVITE_DEFAULTS.server,
                ...matched,
                region: matched.region || matched.location || 'EU West',
                owner: matched.owner || matched.owner_name || 'Server admin'
            };
        }
    } catch (error) {
        console.warn('Invite page failed to load directory data:', error);
    }

    return fallback;
}

function renderInvite(server) {
    const name = server.name || INVITE_DEFAULTS.server.name;
    const description = server.description || INVITE_DEFAULTS.server.description;
    const host = server.host || INVITE_DEFAULTS.server.host;
    const port = server.port || INVITE_DEFAULTS.server.port;
    const address = `${host}:${port}`;
    const online = Boolean(server.online);

    document.title = `Invite - ${name}`;
    document.getElementById('invite-title').textContent = `Join ${name}`;
    document.getElementById('invite-server-name').textContent = name;
    document.getElementById('invite-server-description').textContent = description;
    document.getElementById('invite-address').textContent = address;
    document.getElementById('invite-manual-address').textContent = address;
    document.getElementById('invite-region').textContent = server.region || 'EU West';
    document.getElementById('invite-owner').textContent = server.owner || 'Server admin';

    const statusBadge = document.getElementById('invite-status-badge');
    statusBadge.textContent = online ? 'Online' : 'Offline';
    statusBadge.classList.toggle('invite-badge--online', online);
    statusBadge.classList.toggle('invite-badge--offline', !online);

    const openButton = document.getElementById('invite-open-btn');
    openButton.dataset.host = host;
    openButton.dataset.port = String(port);

    document.getElementById('invite-copy-address-btn').dataset.address = address;
}

function showInviteEmailMessage(message, type) {
    const feedback = document.getElementById('invite-email-feedback');
    feedback.textContent = message;
    feedback.className = `invite-status-message ${type}`;
}

function copyInviteText(text, button, doneLabel) {
    navigator.clipboard.writeText(text).then(() => {
        if (!button) {
            return;
        }

        const original = button.textContent;
        button.textContent = doneLabel;
        setTimeout(() => {
            button.textContent = original;
        }, 1800);
    });
}

function showInviteProtocolFallback(host, port) {
    closeInviteModal();

    const address = `${host}:${port}`;
    const modalHtml = `
        <div class="modal-overlay" id="invite-protocol-modal">
            <div class="modal">
                <h3>Open Grotto invite</h3>
                <p>The protocol handler did not open. You can still install the client or connect manually.</p>
                <div class="modal-options">
                    <div class="modal-option">
                        <h4>Download Grotto</h4>
                        <p>Install the desktop or mobile client and try the invite again.</p>
                        <a href="index.html#download" class="btn btn--primary" onclick="closeInviteModal()">Go to downloads</a>
                    </div>
                    <div class="modal-option">
                        <h4>Connect manually</h4>
                        <p>Use this address in the client:</p>
                        <code class="address-box">${escapeInviteHtml(address)}</code>
                        <button class="btn btn--ghost" type="button" onclick="copyInviteManualAddress(this)">Copy address</button>
                    </div>
                </div>
                <button class="modal-close" type="button" aria-label="Close" onclick="closeInviteModal()">&times;</button>
            </div>
        </div>
    `;

    document.body.insertAdjacentHTML('beforeend', modalHtml);
}

function closeInviteModal() {
    const modal = document.getElementById('invite-protocol-modal');
    if (modal) {
        modal.remove();
    }
}

function copyInviteManualAddress(button) {
    const address = button.previousElementSibling.textContent;
    copyInviteText(address, button, 'Copied');
}

function handleInviteOpen(event) {
    const button = event.currentTarget;
    const host = button.dataset.host;
    const port = button.dataset.port;

    if (!host || !port) {
        return;
    }

    window.location.href = `ircord://${host}:${port}`;

    setTimeout(() => {
        if (!document.hidden) {
            showInviteProtocolFallback(host, port);
        }
    }, 500);

    event.preventDefault();
}

function handleInviteEmailSubmit(event) {
    event.preventDefault();

    const input = document.getElementById('invite-email-input');
    const value = input.value.trim();

    if (!value) {
        showInviteEmailMessage('Enter an email address first.', 'error');
        return;
    }

    if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(value)) {
        showInviteEmailMessage('Invalid email address.', 'error');
        return;
    }

    showInviteEmailMessage('Mock only: invite request captured. SMTP and CAPTCHA backend comes next.', 'success');
    input.value = '';
}

function bindInviteActions() {
    const openButton = document.getElementById('invite-open-btn');
    const copyAddressButton = document.getElementById('invite-copy-address-btn');
    const copyLinkButton = document.getElementById('invite-copy-link-btn');
    const emailForm = document.getElementById('invite-email-form');

    openButton.addEventListener('click', handleInviteOpen);

    copyAddressButton.addEventListener('click', () => {
        copyInviteText(copyAddressButton.dataset.address || '', copyAddressButton, 'Copied address');
    });

    copyLinkButton.addEventListener('click', () => {
        copyInviteText(window.location.href, copyLinkButton, 'Copied link');
    });

    emailForm.addEventListener('submit', handleInviteEmailSubmit);
}

function escapeInviteHtml(text) {
    const div = document.createElement('div');
    div.textContent = String(text || '');
    return div.innerHTML;
}

document.addEventListener('DOMContentLoaded', async () => {
    const server = await resolveInviteServer();
    renderInvite(server);
    bindInviteActions();
});

window.closeInviteModal = closeInviteModal;
window.copyInviteManualAddress = copyInviteManualAddress;
