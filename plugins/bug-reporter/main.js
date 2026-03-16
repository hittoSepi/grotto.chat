// Bug Reporter Bot - Collects bug reports via /bug command

const BUG_REPORT_FILE = "bugs.json";
const MAX_BUG_LENGTH = 2000;
const MIN_BUG_LENGTH = 10;

// Load existing bugs or start fresh
let bugs = [];
let bugIdCounter = 1;

function loadBugs() {
    try {
        const data = grotto.fs.readFile(BUG_REPORT_FILE);
        bugs = JSON.parse(data);
        // Find highest ID for counter
        for (const bug of bugs) {
            if (bug.id >= bugIdCounter) {
                bugIdCounter = bug.id + 1;
            }
        }
        grotto.log.info("Loaded " + bugs.length + " existing bug reports");
    } catch (e) {
        grotto.log.info("No existing bug reports found, starting fresh");
        bugs = [];
    }
}

function saveBugs() {
    try {
        grotto.fs.writeFile(BUG_REPORT_FILE, JSON.stringify(bugs, null, 2));
    } catch (e) {
        grotto.log.error("Failed to save bug reports: " + e.message);
    }
}

// Register /bug command
grotto.onCommand("/bug", {
    description: "Report a bug",
    usage: "/bug <description>"
}, (ctx) => {
    const description = ctx.args.join(" ").trim();
    const sender = ctx.sender;
    const channel = ctx.channel;

    // Validate input
    if (!description) {
        grotto.sendMessage(channel,
            "🐛 " + sender + ": Please provide a bug description. Usage: /bug <description>");
        return;
    }

    if (description.length < MIN_BUG_LENGTH) {
        grotto.sendMessage(channel,
            "🐛 " + sender + ": Bug description too short (min " + MIN_BUG_LENGTH + " chars)");
        return;
    }

    if (description.length > MAX_BUG_LENGTH) {
        grotto.sendMessage(channel,
            "🐛 " + sender + ": Bug description too long (max " + MAX_BUG_LENGTH + " chars)");
        return;
    }

    // Create bug report
    const bug = {
        id: bugIdCounter++,
        reporter: sender,
        channel: channel,
        description: description,
        timestamp: Date.now(),
        status: "open"
    };

    bugs.push(bug);
    saveBugs();

    // Confirm to user
    grotto.sendMessage(channel,
        "🐛 " + sender + ": Bug report #" + bug.id + " received! Thank you for your feedback.");

    grotto.log.info("Bug #" + bug.id + " reported by " + sender + ": " + description.substring(0, 50) + "...");
});

// /bugs command to list recent bugs
grotto.onCommand("/bugs", {
    description: "List recent bug reports",
    usage: "/bugs [count]"
}, (ctx) => {
    const sender = ctx.sender;
    const channel = ctx.channel;
    const count = parseInt(ctx.args[0]) || 5;

    if (bugs.length === 0) {
        grotto.sendMessage(channel, "🐛 " + sender + ": No bug reports yet.");
        return;
    }

    // Show last N bugs
    const recent = bugs.slice(-Math.min(count, 10)).reverse();
    let response = "🐛 Recent bug reports:\n";

    for (const bug of recent) {
        const date = new Date(bug.timestamp).toLocaleDateString();
        const preview = bug.description.substring(0, 40).replace(/\n/g, " ");
        const suffix = bug.description.length > 40 ? "..." : "";
        response += "  #" + bug.id + " [" + bug.status + "] by " + bug.reporter + " (" + date + "): " + preview + suffix + "\n";
    }

    grotto.sendMessage(channel, response.trimEnd());
});

// Initialize
loadBugs();
grotto.log.info("Bug Reporter Bot loaded!");
