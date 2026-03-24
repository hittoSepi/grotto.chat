// Bug Reporter Client Extension
// Captures /bug command and sends to server

const MIN_BUG_LENGTH = 10;
const MAX_BUG_LENGTH = 2000;

// Register /bug command
grotto.onCommand("/bug", {
    description: "Report a bug",
    usage: "/bug <description>"
}, (ctx) => {
    const description = ctx.args.join(" ").trim();
    const sender = ctx.sender;

    // Validate
    if (!description) {
        grotto.client.notify("🐛 Please provide a bug description. Usage: /bug <description>");
        return;
    }

    if (description.length < MIN_BUG_LENGTH) {
        grotto.client.notify("🐛 Bug description too short (min " + MIN_BUG_LENGTH + " chars)");
        return;
    }

    if (description.length > MAX_BUG_LENGTH) {
        grotto.client.notify("🐛 Bug description too long (max " + MAX_BUG_LENGTH + " chars)");
        return;
    }

    // Send to server
    try {
        grotto.client.sendBugReport(description);
        grotto.client.notify("🐛 Bug report sent! Thank you for your feedback.");
    } catch (e) {
        grotto.client.notify("🐛 Failed to send bug report: " + e.message);
    }
});

grotto.log.info("Bug Reporter Client Extension loaded!");
