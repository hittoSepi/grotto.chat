// Bug Reporter Server Extension
// Receives bug reports from clients and stores them

const BUG_REPORT_FILE = "bugs.json";
const MAX_BUGS = 1000;

let bugs = [];
let bugIdCounter = 1;

function loadBugs() {
    try {
        const data = grotto.fs.readFile(BUG_REPORT_FILE);
        bugs = JSON.parse(data);
        for (const bug of bugs) {
            if (bug.id >= bugIdCounter) {
                bugIdCounter = bug.id + 1;
            }
        }
        grotto.log.info("Loaded " + bugs.length + " bug reports");
    } catch (e) {
        grotto.log.info("Starting with empty bug database");
        bugs = [];
    }
}

function saveBugs() {
    try {
        grotto.fs.writeFile(BUG_REPORT_FILE, JSON.stringify(bugs, null, 2));
    } catch (e) {
        grotto.log.error("Failed to save bugs: " + e.message);
    }
}

// Register bug report handler
grotto.server.onBugReport((userId, description) => {
    grotto.log.info("Bug report from " + userId + ": " + description.substring(0, 50) + "...");

    const bug = {
        id: bugIdCounter++,
        reporter: userId,
        description: description,
        timestamp: Date.now(),
        status: "open"
    };

    bugs.push(bug);

    // Limit max bugs
    if (bugs.length > MAX_BUGS) {
        bugs.shift(); // Remove oldest
    }

    saveBugs();
});

loadBugs();
grotto.log.info("Bug Reporter Server Extension loaded!");
