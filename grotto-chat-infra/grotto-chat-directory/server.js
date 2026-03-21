/**
 * Grotto Directory Service
 * 
 * Manages the public listing of Grotto servers.
 * Servers register themselves and send periodic pings to stay listed.
 */

const express = require('express');
const cors = require('cors');
const { v4: uuidv4 } = require('uuid');
const path = require('path');
const fs = require('fs');

const app = express();
const PORT = process.env.PORT || 3000;

// Data file for persistence
const DATA_DIR = process.env.DATA_DIR || './data';
const DATA_FILE = path.join(DATA_DIR, 'servers.json');

// In-memory storage for registered servers
const servers = new Map();

// Server timeout - remove servers that haven't pinged in 10 minutes
const SERVER_TIMEOUT_MS = 10 * 60 * 1000;

// Cleanup interval - check for stale servers every minute
const CLEANUP_INTERVAL_MS = 60 * 1000;

// Persist data to disk
async function saveData() {
    try {
        const data = Array.from(servers.values());
        await fs.promises.writeFile(DATA_FILE, JSON.stringify(data, null, 2));
    } catch (err) {
        console.error('Failed to save data:', err.message);
    }
}

// Load data from disk
async function loadData() {
    try {
        await fs.promises.access(DATA_FILE);
        const file_contents = await fs.promises.readFile(DATA_FILE, 'utf8');
        const data = JSON.parse(file_contents);
        for (const server of data) {
            servers.set(server.id, server);
        }
        console.log(`Loaded ${servers.size} servers from disk`);
    } catch (err) {
        if (err.code === 'ENOENT') {
            return;
        }
        console.error('Failed to load data:', err.message);
    }
}

// Ensure data directory exists
if (!fs.existsSync(DATA_DIR)) {
    fs.mkdirSync(DATA_DIR, { recursive: true });
}

// Middleware
app.use(cors({
    origin: true,
    methods: ['GET', 'POST', 'OPTIONS'],
    allowedHeaders: ['Content-Type']
}));
app.options('/api/*', cors({
    origin: true,
    methods: ['GET', 'POST', 'OPTIONS'],
    allowedHeaders: ['Content-Type']
}));
app.use('/api', (req, res, next) => {
    res.header('Access-Control-Allow-Origin', req.headers.origin || '*');
    res.header('Vary', 'Origin');
    res.header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.header('Access-Control-Allow-Headers', 'Content-Type');

    if (req.method === 'OPTIONS') {
        return res.sendStatus(204);
    }

    next();
});
app.use(express.json());

// Request logging middleware
app.use((req, res, next) => {
    const timestamp = new Date().toISOString();
    console.log(`[${timestamp}] ${req.method} ${req.path} - ${req.ip}`);
    next();
});

/**
 * API Routes
 */

// Health check
app.get('/api/health', (req, res) => {
    res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// Register a new server
app.post('/api/servers/register', async (req, res) => {
    const { host, port, name, description } = req.body;

    // Validate required fields
    if (!host || !port) {
        return res.status(400).json({
            error: 'Missing required fields: host and port are required'
        });
    }

    // Validate port is a number
    const portNum = parseInt(port, 10);
    if (isNaN(portNum) || portNum < 1 || portNum > 65535) {
        return res.status(400).json({
            error: 'Invalid port number'
        });
    }

    // Generate unique server ID
    const serverId = uuidv4();
    const serverAddress = `${host}:${portNum}`;

    // Check if server is already registered (same host:port)
    for (const [id, server] of servers.entries()) {
        if (server.address === serverAddress) {
            // Update existing server
            server.lastPing = Date.now();
            server.name = name || server.name;
            server.description = description || server.description;
            console.log(`Server re-registered: ${serverAddress} (ID: ${id})`);
            return res.json({
                server_id: id,
                message: 'Server re-registered successfully'
            });
        }
    }

    // Register new server
    const serverInfo = {
        id: serverId,
        address: serverAddress,
        host: host,
        port: portNum,
        name: name || 'Grotto Server',
        description: description || 'An Grotto encrypted chat server',
        registeredAt: Date.now(),
        lastPing: Date.now(),
        online: true
    };

    servers.set(serverId, serverInfo);
    console.log(`New server registered: ${serverAddress} (ID: ${serverId})`);
    
    await saveData();

    res.status(201).json({
        server_id: serverId,
        message: 'Server registered successfully'
    });
});

// Ping to keep server alive
app.post('/api/servers/ping', async (req, res) => {
    const { server_id } = req.body;

    if (!server_id) {
        return res.status(400).json({
            error: 'Missing required field: server_id'
        });
    }

    const server = servers.get(server_id);
    if (!server) {
        return res.status(404).json({
            error: 'Server not found. Please register first.'
        });
    }

    // Update last ping time
    server.lastPing = Date.now();
    server.online = true;
    
    await saveData();

    res.json({
        message: 'Ping received',
        server_id: server_id
    });
});

// Unregister a server
app.post('/api/servers/unregister', async (req, res) => {
    const { server_id } = req.body;

    if (!server_id) {
        return res.status(400).json({
            error: 'Missing required field: server_id'
        });
    }

    const server = servers.get(server_id);
    if (!server) {
        return res.status(404).json({
            error: 'Server not found'
        });
    }

    servers.delete(server_id);
    console.log(`Server unregistered: ${server.address} (ID: ${server_id})`);
    
    await saveData();

    res.json({
        message: 'Server unregistered successfully'
    });
});

// Get list of all online public servers
app.get('/api/servers', (req, res) => {
    const serverList = [];
    const now = Date.now();

    for (const [id, server] of servers.entries()) {
        // Only include online servers that have pinged recently
        const isOnline = server.online && (now - server.lastPing) < SERVER_TIMEOUT_MS;
        
        serverList.push({
            id: id,
            host: server.host,
            port: server.port,
            name: server.name,
            description: server.description,
            online: isOnline,
            lastPing: new Date(server.lastPing).toISOString()
        });
    }

    // Sort by name
    serverList.sort((a, b) => a.name.localeCompare(b.name));

    res.json({
        count: serverList.length,
        servers: serverList
    });
});

// Get specific server info
app.get('/api/servers/:id', (req, res) => {
    const server = servers.get(req.params.id);
    
    if (!server) {
        return res.status(404).json({ error: 'Server not found' });
    }

    const now = Date.now();
    const isOnline = server.online && (now - server.lastPing) < SERVER_TIMEOUT_MS;

    res.json({
        id: server.id,
        host: server.host,
        port: server.port,
        name: server.name,
        description: server.description,
        online: isOnline,
        registeredAt: new Date(server.registeredAt).toISOString(),
        lastPing: new Date(server.lastPing).toISOString()
    });
});

/**
 * Cleanup job - remove stale servers
 */
async function cleanupStaleServers() {
    const now = Date.now();
    let removedCount = 0;

    for (const [id, server] of servers.entries()) {
        if (now - server.lastPing > SERVER_TIMEOUT_MS) {
            server.online = false;
            // Actually remove after 2x timeout
            if (now - server.lastPing > SERVER_TIMEOUT_MS * 2) {
                servers.delete(id);
                removedCount++;
                console.log(`Removed stale server: ${server.address} (ID: ${id})`);
            }
        }
    }

    if (removedCount > 0) {
        console.log(`Cleanup: removed ${removedCount} stale servers`);
        await saveData();
    }
}

// Start cleanup interval
setInterval(cleanupStaleServers, CLEANUP_INTERVAL_MS);

/**
 * Serve static files (landing page)
 */
app.use(express.static(path.join(__dirname, 'public')));

// Fallback to index.html for SPA routes
app.get('*', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

/**
 * Error handling
 */
app.use((err, req, res, next) => {
    console.error('Error:', err);
    res.status(500).json({ error: 'Internal server error' });
});

/**
 * Start server
 */
app.listen(PORT, async () => {
    // Load persisted data
    await loadData();
    
    console.log('='.repeat(50));
    console.log('Grotto Directory Service');
    console.log('='.repeat(50));
    console.log(`Server running on port ${PORT}`);
    console.log(`Data directory: ${DATA_DIR}`);
    console.log(`API endpoints:`);
    console.log(`  POST /api/servers/register  - Register a new server`);
    console.log(`  POST /api/servers/ping      - Keep server alive`);
    console.log(`  POST /api/servers/unregister - Remove server`);
    console.log(`  GET  /api/servers           - List all servers`);
    console.log(`  GET  /api/servers/:id       - Get specific server`);
    console.log(`  GET  /api/health            - Health check`);
    console.log('='.repeat(50));
});
