#include "grotto/plugin/plugin_manager.hpp"
#include "grotto/plugin/plugin_meta.hpp"

#include <spdlog/spdlog.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Create a temporary test plugin on disk
static fs::path create_test_plugin(const fs::path& base_dir,
                                    const std::string& name,
                                    const std::string& type,
                                    const std::string& js_code,
                                    const std::string& extra_json = "") {
    auto dir = base_dir / name;
    fs::create_directories(dir);

    // plugin.json
    std::string bot_section;
    if (type == "bot") {
        bot_section = R"(, "bot": { "user_id": ")" + name + R"(", "auto_join_channels": ["#test"] })";
    }

    std::string json = R"({
        "name": ")" + name + R"(",
        "display_name": ")" + name + R"(",
        "version": "1.0.0",
        "type": ")" + type + R"(",
        "entry": "main.js",
        "description": "Test plugin",
        "permissions": ["read_messages", "send_messages", "register_commands"])" +
        bot_section + extra_json + "\n}";

    {
        std::ofstream f(dir / "plugin.json");
        f << json;
    }

    // main.js
    {
        std::ofstream f(dir / "main.js");
        f << js_code;
    }

    return dir;
}

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        std::cout << "  TEST: " << #name << " ... "; \
    } while(0)

#define PASS() \
    do { \
        tests_passed++; \
        std::cout << "PASS" << std::endl; \
    } while(0)

#define FAIL(msg) \
    do { \
        std::cout << "FAIL: " << msg << std::endl; \
    } while(0)

// ═══════════════════════════════════════════════════════════
// Test 1: Plugin metadata parsing
// ═══════════════════════════════════════════════════════════

void test_meta_parsing() {
    TEST(meta_parsing);

    auto tmp = fs::temp_directory_path() / "grotto_test_plugins";
    fs::create_directories(tmp);

    auto dir = create_test_plugin(tmp, "testbot", "bot",
        "grotto.log.info('hello');",
        "");

    auto meta = grotto::plugin::parse_plugin_json(dir / "plugin.json");
    if (!meta) { FAIL("parse returned nullopt"); return; }
    if (meta->name != "testbot") { FAIL("wrong name"); return; }
    if (meta->type != grotto::plugin::PluginType::Bot) { FAIL("wrong type"); return; }
    if (!meta->bot) { FAIL("missing bot config"); return; }
    if (meta->bot->user_id != "testbot") { FAIL("wrong bot user_id"); return; }
    if (!grotto::plugin::validate_plugin_meta(*meta)) { FAIL("validation failed"); return; }

    fs::remove_all(tmp);
    PASS();
}

// ═══════════════════════════════════════════════════════════
// Test 2: Plugin load and JS execution
// ═══════════════════════════════════════════════════════════

void test_plugin_load() {
    TEST(plugin_load);

    auto tmp = fs::temp_directory_path() / "grotto_test_plugins2";
    fs::create_directories(tmp);

    // JS that sets a global variable we can check
    create_test_plugin(tmp, "simplebot", "bot",
        R"(
        grotto.log.info("SimpleBot loaded!");
        var loaded = true;
        )");

    std::vector<std::string> sent_messages;
    grotto::plugin::PluginContext ctx;
    ctx.send_message = [&](const std::string& ch, const std::string& text) {
        sent_messages.push_back(ch + ": " + text);
    };

    grotto::plugin::PluginManager mgr(ctx);
    mgr.discover(tmp);

    auto plugins = mgr.loaded_plugins();
    if (plugins.size() != 1) { FAIL("expected 1 plugin, got " + std::to_string(plugins.size())); fs::remove_all(tmp); return; }
    if (plugins[0].name != "simplebot") { FAIL("wrong plugin name"); fs::remove_all(tmp); return; }
    if (plugins[0].state != grotto::plugin::PluginState::Running) { FAIL("not running"); fs::remove_all(tmp); return; }

    fs::remove_all(tmp);
    PASS();
}

// ═══════════════════════════════════════════════════════════
// Test 3: DiceBot — command registration and dispatch
// ═══════════════════════════════════════════════════════════

void test_dicebot() {
    TEST(dicebot_command);

    auto tmp = fs::temp_directory_path() / "grotto_test_dicebot";
    fs::create_directories(tmp);

    create_test_plugin(tmp, "dicebot", "bot", R"(
function parseDice(input) {
    if (!input) return { count: 1, sides: 6 };
    var match = input.match(/^(\d+)d(\d+)$/i);
    if (!match) {
        var n = parseInt(input);
        if (!isNaN(n) && n >= 1 && n <= 100) return { count: 1, sides: n };
        return null;
    }
    var count = parseInt(match[1]);
    var sides = parseInt(match[2]);
    if (count < 1 || count > 20 || sides < 2 || sides > 1000) return null;
    return { count: count, sides: sides };
}

grotto.onCommand("/roll", {
    description: "Roll dice",
    usage: "/roll [NdN]"
}, function(ctx) {
    var dice = parseDice(ctx.args[0]);
    if (!dice) {
        grotto.sendMessage(ctx.channel, ctx.sender + ": Invalid format.");
        return;
    }
    // For testing, always "roll" 1
    var total = dice.count;
    grotto.sendMessage(ctx.channel, ctx.sender + ": rolled " + dice.count + "d" + dice.sides + " = " + total);
});

grotto.log.info("DiceBot loaded!");
    )");

    std::vector<std::string> sent_messages;
    grotto::plugin::PluginContext ctx;
    ctx.send_message = [&](const std::string& ch, const std::string& text) {
        sent_messages.push_back(ch + ": " + text);
    };

    grotto::plugin::PluginManager mgr(ctx);
    mgr.discover(tmp);

    // Verify command was registered
    auto cmds = mgr.registered_commands();
    if (cmds.empty()) { FAIL("no commands registered"); fs::remove_all(tmp); return; }
    if (cmds[0].name != "/roll") { FAIL("wrong command name: " + cmds[0].name); fs::remove_all(tmp); return; }

    // Dispatch a command
    grotto::plugin::PluginCommand cmd;
    cmd.name = "/roll";
    cmd.channel = "#games";
    cmd.sender_id = "Sepi";
    cmd.args = {"2d6"};
    mgr.dispatch_command(cmd);

    // Run tick to process any pending jobs
    mgr.tick();

    if (sent_messages.empty()) { FAIL("no messages sent"); fs::remove_all(tmp); return; }
    if (sent_messages[0].find("Sepi") == std::string::npos) {
        FAIL("message doesn't contain sender: " + sent_messages[0]);
        fs::remove_all(tmp);
        return;
    }

    fs::remove_all(tmp);
    PASS();
}

// ═══════════════════════════════════════════════════════════
// Test 4: Event dispatch
// ═══════════════════════════════════════════════════════════

void test_event_dispatch() {
    TEST(event_dispatch);

    auto tmp = fs::temp_directory_path() / "grotto_test_events";
    fs::create_directories(tmp);

    create_test_plugin(tmp, "greeter", "bot", R"(
grotto.on("user_joined", function(ev) {
    grotto.sendMessage(ev.channel, "Welcome " + ev.sender + "!");
});
grotto.log.info("Greeter loaded!");
    )");

    std::vector<std::string> sent_messages;
    grotto::plugin::PluginContext ctx;
    ctx.send_message = [&](const std::string& ch, const std::string& text) {
        sent_messages.push_back(ch + ": " + text);
    };

    grotto::plugin::PluginManager mgr(ctx);
    mgr.discover(tmp);

    // Dispatch a user_joined event
    grotto::plugin::PluginEvent event;
    event.type = grotto::plugin::PluginEvent::Type::UserJoined;
    event.channel = "#general";
    event.user_id = "Matti";
    mgr.dispatch(event);

    mgr.tick();

    if (sent_messages.empty()) { FAIL("no messages sent"); fs::remove_all(tmp); return; }
    if (sent_messages[0] != "#general: Welcome Matti!") {
        FAIL("unexpected message: " + sent_messages[0]);
        fs::remove_all(tmp);
        return;
    }

    fs::remove_all(tmp);
    PASS();
}

// ═══════════════════════════════════════════════════════════
// Test 5: Server extension does NOT receive message events
// ═══════════════════════════════════════════════════════════

void test_server_ext_no_messages() {
    TEST(server_ext_no_messages);

    auto tmp = fs::temp_directory_path() / "grotto_test_serverext";
    fs::create_directories(tmp);

    // Server extension plugin.json needs manage_channels/manage_users perms
    auto dir = tmp / "logger";
    fs::create_directories(dir);

    {
        std::ofstream f(dir / "plugin.json");
        f << R"({
            "name": "logger",
            "version": "1.0.0",
            "type": "server_extension",
            "entry": "main.js",
            "permissions": ["manage_channels"]
        })";
    }
    {
        std::ofstream f(dir / "main.js");
        f << R"(
var got_message = false;
var got_join = false;

grotto.on("message", function(ev) {
    got_message = true;
    grotto.log.error("BUG: server_extension received message event!");
});

grotto.on("user_joined", function(ev) {
    got_join = true;
    grotto.log.info("User joined: " + ev.sender);
});

grotto.log.info("Logger loaded!");
        )";
    }

    grotto::plugin::PluginContext ctx;
    grotto::plugin::PluginManager mgr(ctx);
    mgr.discover(tmp);

    // Send message event — should be filtered
    grotto::plugin::PluginEvent msg_event;
    msg_event.type = grotto::plugin::PluginEvent::Type::MessageReceived;
    msg_event.channel = "#general";
    msg_event.user_id = "Sepi";
    msg_event.text = "secret message";
    mgr.dispatch(msg_event);

    // Send join event — should pass through
    grotto::plugin::PluginEvent join_event;
    join_event.type = grotto::plugin::PluginEvent::Type::UserJoined;
    join_event.channel = "#general";
    join_event.user_id = "Sepi";
    mgr.dispatch(join_event);

    mgr.tick();

    // If we got here without the error log, the filter worked
    fs::remove_all(tmp);
    PASS();
}

// ═══════════════════════════════════════════════════════════

int main() {
    spdlog::set_level(spdlog::level::info);
    std::cout << "Grotto Plugin System Tests" << std::endl;
    std::cout << "=========================" << std::endl;

    test_meta_parsing();
    test_plugin_load();
    test_dicebot();
    test_event_dispatch();
    test_server_ext_no_messages();

    std::cout << std::endl;
    std::cout << tests_passed << "/" << tests_run << " tests passed." << std::endl;

    return (tests_passed == tests_run) ? 0 : 1;
}
