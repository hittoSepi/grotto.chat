# Grotto Server TUI — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a TUI admin interface for grotto-server as a separate static library (`grotto-server-tui/`) with a standalone executable (`grotto-tui`) that can attach/detach to a running server via local socket.

**Architecture:** Server always starts an `AdminSocketListener` on a local socket (named pipe on Windows, Unix socket on Linux). The `grotto-tui` executable connects to it and renders an FTXUI-based admin UI. In integrated mode, the server spawns the TUI logic in-process. Admin protocol uses 4-byte BE length prefix + JSON messages.

**Tech Stack:** C++20, FTXUI, nlohmann/json, Boost.Asio, spdlog

**Design doc:** `docs/plans/2026-03-15-server-tui-design.md`

---

## Task 1: Project Scaffold & CMake

**Files:**
- Create: `grotto-server-tui/CMakeLists.txt`
- Create: `grotto-server-tui/include/grotto/tui/admin_tui.hpp` (stub)
- Create: `grotto-server-tui/src/admin_tui.cpp` (stub)
- Create: `grotto-server-tui/src/main.cpp` (stub executable)

**Step 1: Create CMakeLists.txt**

Follow `grotto-plugin/CMakeLists.txt` pattern. Use FetchContent for FTXUI and nlohmann/json. Boost.Asio comes from vcpkg (same as server).

```cmake
cmake_minimum_required(VERSION 3.20)

project(grotto-server-tui VERSION 0.1.0 DESCRIPTION "Grotto Server TUI" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# ── FetchContent ──────────────────────────────
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# ── FTXUI ─────────────────────────────────────
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v5.0.0
    GIT_SHALLOW    TRUE
)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

# ── nlohmann/json ─────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# ── spdlog ────────────────────────────────────
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.1
    GIT_SHALLOW    TRUE
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# ── Boost (from vcpkg or system) ──────────────
find_package(Boost REQUIRED COMPONENTS system)

# ── Static library ────────────────────────────
set(TUI_SOURCES
    src/admin_tui.cpp
    src/admin_protocol.cpp
    src/admin_socket_client.cpp
    src/admin_socket_listener.cpp
    src/log_view.cpp
    src/user_view.cpp
    src/settings_view.cpp
    src/bugreport_view.cpp
)

add_library(grotto-server-tui STATIC ${TUI_SOURCES})

target_include_directories(grotto-server-tui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(grotto-server-tui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(grotto-server-tui PUBLIC
    ftxui::screen
    ftxui::dom
    ftxui::component
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Boost::system
)

if(MSVC)
    target_compile_options(grotto-server-tui PRIVATE /W4 /WX-)
else()
    target_compile_options(grotto-server-tui PRIVATE -Wall -Wextra)
endif()

# ── Standalone executable ─────────────────────
add_executable(grotto-tui src/main.cpp)
target_link_libraries(grotto-tui PRIVATE grotto-server-tui)
```

**Step 2: Create stub header**

```cpp
// include/grotto/tui/admin_tui.hpp
#pragma once

namespace grotto::tui {

class AdminTui {
public:
    int run();
};

} // namespace grotto::tui
```

**Step 3: Create stub source**

```cpp
// src/admin_tui.cpp
#include <grotto/tui/admin_tui.hpp>
#include <spdlog/spdlog.h>

namespace grotto::tui {

int AdminTui::run() {
    spdlog::info("AdminTui starting...");
    return 0;
}

} // namespace grotto::tui
```

**Step 4: Create stub main.cpp**

```cpp
// src/main.cpp
#include <grotto/tui/admin_tui.hpp>

int main() {
    grotto::tui::AdminTui tui;
    return tui.run();
}
```

**Step 5: Create empty stubs for all other source files**

Create empty `.cpp` files for: `admin_protocol.cpp`, `admin_socket_client.cpp`, `admin_socket_listener.cpp`, `log_view.cpp`, `user_view.cpp`, `settings_view.cpp`, `bugreport_view.cpp`. Each with just an empty namespace block.

**Step 6: Build and verify**

```bash
cd grotto-server-tui
cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

Expected: Compiles successfully, produces `grotto-tui` executable and `grotto-server-tui` static library.

**Step 7: Commit**

```bash
git add grotto-server-tui/
git commit -m "feat(tui): scaffold grotto-server-tui project with CMake, FTXUI, stubs"
```

---

## Task 2: Admin Protocol — JSON Message Types

**Files:**
- Create: `grotto-server-tui/include/grotto/tui/admin_protocol.hpp`
- Modify: `grotto-server-tui/src/admin_protocol.cpp`
- Create: `grotto-server-tui/tests/test_protocol.cpp`

This task defines the shared data types and JSON serialization for the admin protocol. No socket code yet — pure data + serialize/deserialize.

**Step 1: Define protocol types in header**

```cpp
// include/grotto/tui/admin_protocol.hpp
#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace grotto::tui::protocol {

// ── Server -> TUI events ─────────────────────

struct LogEntry {
    std::string level;   // "debug", "info", "warn", "error"
    std::string msg;
    std::string ts;      // ISO 8601
};

struct UserInfo {
    std::string id;
    std::string ip;
    std::string nickname;
    std::string connected;  // ISO 8601
};

struct ChannelInfo {
    std::string name;
    int members = 0;
};

struct ServerStats {
    int64_t uptime = 0;      // seconds
    int connections = 0;
    std::string version;
};

struct BugReport {
    int id = 0;
    std::string user;
    std::string description;
    std::string status;       // "new", "read", "resolved"
    std::string created;      // ISO 8601
};

struct ConfigEntry {
    std::string key;
    std::string value;
    bool read_only = false;   // true = requires restart
};

// ── TUI -> Server commands ───────────────────

struct KickCommand {
    std::string user_id;
    std::string reason;
};

struct BanCommand {
    std::string user_id;
    std::string reason;
};

struct SetConfigCommand {
    std::string key;
    std::string value;
};

struct UpdateBugReportCommand {
    int id = 0;
    std::string status;
};

// ── Framing ──────────────────────────────────

// Encode a JSON message with 4-byte BE length prefix
std::vector<uint8_t> encode_frame(const nlohmann::json& msg);

// Decode length prefix, returns payload size.
// Returns 0 if buffer has < 4 bytes.
uint32_t decode_frame_length(const uint8_t* data, size_t len);

// ── Serialization helpers ────────────────────

nlohmann::json to_json_event(const std::string& type, const nlohmann::json& data);
nlohmann::json to_json_command(const std::string& cmd, const nlohmann::json& params);

// JSON conversion macros via nlohmann
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LogEntry, level, msg, ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UserInfo, id, ip, nickname, connected)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChannelInfo, name, members)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ServerStats, uptime, connections, version)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BugReport, id, user, description, status, created)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ConfigEntry, key, value, read_only)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(KickCommand, user_id, reason)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BanCommand, user_id, reason)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetConfigCommand, key, value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UpdateBugReportCommand, id, status)

} // namespace grotto::tui::protocol
```

**Step 2: Implement framing + helpers**

```cpp
// src/admin_protocol.cpp
#include <grotto/tui/admin_protocol.hpp>
#include <boost/endian/conversion.hpp>

namespace grotto::tui::protocol {

std::vector<uint8_t> encode_frame(const nlohmann::json& msg) {
    std::string payload = msg.dump();
    uint32_t len = boost::endian::native_to_big(static_cast<uint32_t>(payload.size()));

    std::vector<uint8_t> frame(4 + payload.size());
    std::memcpy(frame.data(), &len, 4);
    std::memcpy(frame.data() + 4, payload.data(), payload.size());
    return frame;
}

uint32_t decode_frame_length(const uint8_t* data, size_t len) {
    if (len < 4) return 0;
    uint32_t net_len;
    std::memcpy(&net_len, data, 4);
    return boost::endian::big_to_native(net_len);
}

nlohmann::json to_json_event(const std::string& type, const nlohmann::json& data) {
    return {{"type", type}, {"data", data}};
}

nlohmann::json to_json_command(const std::string& cmd, const nlohmann::json& params) {
    nlohmann::json j = {{"cmd", cmd}};
    j.update(params);
    return j;
}

} // namespace grotto::tui::protocol
```

**Step 3: Write tests**

```cpp
// tests/test_protocol.cpp
#include <grotto/tui/admin_protocol.hpp>
#include <cassert>
#include <iostream>

using namespace grotto::tui::protocol;

void test_frame_roundtrip() {
    nlohmann::json msg = {{"type", "log"}, {"level", "info"}, {"msg", "hello"}};
    auto frame = encode_frame(msg);

    // Decode length
    uint32_t payload_len = decode_frame_length(frame.data(), frame.size());
    assert(payload_len > 0);
    assert(payload_len == frame.size() - 4);

    // Parse payload
    std::string payload(frame.begin() + 4, frame.end());
    auto decoded = nlohmann::json::parse(payload);
    assert(decoded == msg);

    std::cout << "  PASS: frame roundtrip\n";
}

void test_log_entry_serialization() {
    LogEntry entry{"info", "User connected", "2026-03-15T12:00:00Z"};
    nlohmann::json j = entry;

    assert(j["level"] == "info");
    assert(j["msg"] == "User connected");

    auto back = j.get<LogEntry>();
    assert(back.level == entry.level);
    assert(back.msg == entry.msg);

    std::cout << "  PASS: LogEntry serialization\n";
}

void test_kick_command_serialization() {
    KickCommand cmd{"Jansen", "spammaus"};
    auto j = to_json_command("kick", cmd);

    assert(j["cmd"] == "kick");
    assert(j["user_id"] == "Jansen");
    assert(j["reason"] == "spammaus");

    std::cout << "  PASS: KickCommand serialization\n";
}

void test_user_info_list() {
    std::vector<UserInfo> users = {
        {"Sepi", "1.2.3.4", "Sepi", "2026-03-15T11:50:00Z"},
        {"Mikko", "5.6.7.8", "Mikko", "2026-03-15T11:55:00Z"}
    };
    auto event = to_json_event("users", users);

    assert(event["type"] == "users");
    assert(event["data"].size() == 2);
    assert(event["data"][0]["id"] == "Sepi");

    std::cout << "  PASS: UserInfo list event\n";
}

int main() {
    std::cout << "admin_protocol tests:\n";
    test_frame_roundtrip();
    test_log_entry_serialization();
    test_kick_command_serialization();
    test_user_info_list();
    std::cout << "All tests passed.\n";
    return 0;
}
```

**Step 4: Add test to CMakeLists.txt**

Add to bottom of `grotto-server-tui/CMakeLists.txt`:

```cmake
# ── Tests ─────────────────────────────────────
option(GROTTO_CHAT_TUI_BUILD_TESTS "Build TUI tests" ON)
if(GROTTO_CHAT_TUI_BUILD_TESTS)
    enable_testing()
    add_executable(test-protocol tests/test_protocol.cpp)
    target_link_libraries(test-protocol PRIVATE grotto-server-tui)
    add_test(NAME protocol-tests COMMAND test-protocol)
endif()
```

**Step 5: Build and run tests**

```bash
cd grotto-server-tui/build
cmake --build .
ctest -V
```

Expected: All 4 tests pass.

**Step 6: Commit**

```bash
git commit -m "feat(tui): implement admin protocol JSON types and framing"
```

---

## Task 3: Admin Socket Listener (Server Side)

**Files:**
- Create: `grotto-server-tui/include/grotto/tui/admin_socket_listener.hpp`
- Modify: `grotto-server-tui/src/admin_socket_listener.cpp`

The listener runs inside the server process. It accepts one TUI connection at a time on a local socket and sends events / receives commands via the admin protocol.

**Step 1: Define the listener header**

```cpp
// include/grotto/tui/admin_socket_listener.hpp
#pragma once

#include <grotto/tui/admin_protocol.hpp>
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#include <boost/asio/windows/stream_handle.hpp>
#else
#include <boost/asio/local/stream_protocol.hpp>
#endif

namespace grotto::tui {

// Callback for when TUI sends a command
using CommandCallback = std::function<void(const nlohmann::json& cmd)>;

class AdminSocketListener : public std::enable_shared_from_this<AdminSocketListener> {
public:
    explicit AdminSocketListener(boost::asio::io_context& ioc);
    ~AdminSocketListener();

    // Start listening on the admin socket
    void start();

    // Stop and clean up
    void stop();

    // Send an event to connected TUI client
    void send_event(const nlohmann::json& event);

    // Set callback for incoming commands
    void set_command_callback(CommandCallback cb);

    // Check if a TUI client is connected
    bool has_client() const;

    // Get the socket path for the client to connect to
    static std::string socket_path();

private:
    void do_accept();
    void do_read();
    void process_message(const std::string& payload);

    boost::asio::io_context& ioc_;
    CommandCallback on_command_;
    std::mutex write_mutex_;
    bool running_ = false;

#ifdef _WIN32
    // Windows named pipe implementation
    // (details in .cpp — boost::asio::windows::stream_handle)
    std::string pipe_name_;
    void* pipe_handle_ = nullptr;
    std::unique_ptr<boost::asio::windows::stream_handle> client_stream_;
#else
    // Unix domain socket
    using unix_socket = boost::asio::local::stream_protocol;
    std::unique_ptr<unix_socket::acceptor> acceptor_;
    std::unique_ptr<unix_socket::socket> client_socket_;
#endif

    // Read buffer
    std::vector<uint8_t> read_buf_;
    static constexpr size_t kMaxMsgSize = 65536;
};

} // namespace grotto::tui
```

**Step 2: Implement the listener**

Implement `admin_socket_listener.cpp` with platform-specific socket creation:

- **Linux:** `boost::asio::local::stream_protocol` on `/tmp/grotto-admin.sock` (or `$XDG_RUNTIME_DIR/grotto-admin.sock`)
- **Windows:** Named pipe `\\.\pipe\grotto-admin` via `CreateNamedPipeA` + `boost::asio::windows::stream_handle`

Key behaviors:
- `start()` — create socket/pipe, call `do_accept()`
- `do_accept()` — async accept one client, then call `do_read()` loop
- `do_read()` — read 4-byte length prefix, then payload, parse JSON, call `on_command_`
- `send_event()` — encode frame, async write to client (mutex-protected)
- `stop()` — close socket, remove socket file (Linux)

**Step 3: Build and verify compilation**

```bash
cmake --build build
```

**Step 4: Commit**

```bash
git commit -m "feat(tui): implement AdminSocketListener for server-side admin socket"
```

---

## Task 4: Admin Socket Client (TUI Side)

**Files:**
- Create: `grotto-server-tui/include/grotto/tui/admin_socket_client.hpp`
- Modify: `grotto-server-tui/src/admin_socket_client.cpp`

The client runs in the `grotto-tui` process. It connects to the server's admin socket, receives events, and sends commands.

**Step 1: Define client header**

```cpp
// include/grotto/tui/admin_socket_client.hpp
#pragma once

#include <grotto/tui/admin_protocol.hpp>
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace grotto::tui {

using EventCallback = std::function<void(const nlohmann::json& event)>;

class AdminSocketClient {
public:
    AdminSocketClient();
    ~AdminSocketClient();

    // Connect to admin socket. Returns false if connection fails.
    bool connect(const std::string& path = "");

    // Disconnect
    void disconnect();

    // Send a command to server
    void send_command(const nlohmann::json& cmd);

    // Set callback for incoming events
    void set_event_callback(EventCallback cb);

    // Is connected?
    bool connected() const;

private:
    void io_thread_func();
    void do_read();
    void process_message(const std::string& payload);

    boost::asio::io_context ioc_;
    EventCallback on_event_;
    std::mutex write_mutex_;
    std::thread io_thread_;
    std::atomic<bool> connected_{false};

#ifdef _WIN32
    std::unique_ptr<boost::asio::windows::stream_handle> stream_;
#else
    using unix_socket = boost::asio::local::stream_protocol;
    std::unique_ptr<unix_socket::socket> socket_;
#endif

    std::vector<uint8_t> read_buf_;
};

} // namespace grotto::tui
```

**Step 2: Implement client**

Key behaviors:
- `connect()` — connect to socket path (default: `AdminSocketListener::socket_path()`), start io thread
- `io_thread_func()` — runs `ioc_.run()` in background thread
- `do_read()` — async read loop (same framing as listener)
- `send_command()` — encode frame, async write (mutex-protected)
- `disconnect()` — close socket, stop ioc, join io thread

**Step 3: Build and verify**

```bash
cmake --build build
```

**Step 4: Commit**

```bash
git commit -m "feat(tui): implement AdminSocketClient for TUI-side socket connection"
```

---

## Task 5: Log View Component

**Files:**
- Create: `grotto-server-tui/include/grotto/tui/log_view.hpp`
- Modify: `grotto-server-tui/src/log_view.cpp`

FTXUI component that displays a scrollable, color-coded log list.

**Step 1: Define log view**

```cpp
// include/grotto/tui/log_view.hpp
#pragma once

#include <grotto/tui/admin_protocol.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <deque>
#include <mutex>
#include <string>

namespace grotto::tui {

class LogView {
public:
    LogView();

    // Add a log entry (thread-safe, called from socket thread)
    void push(protocol::LogEntry entry);

    // Clear all entries
    void clear();

    // FTXUI render: returns Element for the log area
    ftxui::Element render();

private:
    ftxui::Color color_for_level(const std::string& level);

    std::deque<protocol::LogEntry> entries_;
    std::mutex mutex_;
    int scroll_offset_ = 0;
    static constexpr size_t kMaxEntries = 10000;
};

} // namespace grotto::tui
```

**Step 2: Implement log view**

Render each entry as colored text:
- `DEBUG` → gray (`Color::GrayDark`)
- `INFO` → white (`Color::White`)
- `WARN` → yellow (`Color::Yellow`)
- `ERROR` → red (`Color::Red`)

Use `vbox` of `hbox` elements: `[timestamp] [LEVEL] message`

Auto-scroll to bottom unless user has scrolled up.

**Step 3: Build and verify**

```bash
cmake --build build
```

**Step 4: Commit**

```bash
git commit -m "feat(tui): implement LogView FTXUI component"
```

---

## Task 6: User View Component + Context Menu

**Files:**
- Create: `grotto-server-tui/include/grotto/tui/user_view.hpp`
- Modify: `grotto-server-tui/src/user_view.cpp`

FTXUI component for the sidebar showing online users and channels. Right-click on a user opens a context menu (Kick, Ban, Whois).

**Step 1: Define user view**

```cpp
// include/grotto/tui/user_view.hpp
#pragma once

#include <grotto/tui/admin_protocol.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace grotto::tui {

using ActionCallback = std::function<void(const std::string& action, const std::string& user_id)>;

class UserView {
public:
    explicit UserView(ActionCallback on_action);

    // Update user list (thread-safe)
    void set_users(std::vector<protocol::UserInfo> users);

    // Update channel list (thread-safe)
    void set_channels(std::vector<protocol::ChannelInfo> channels);

    // FTXUI: get component (handles mouse events)
    ftxui::Component component();

    // FTXUI: render sidebar element
    ftxui::Element render();

private:
    std::vector<protocol::UserInfo> users_;
    std::vector<protocol::ChannelInfo> channels_;
    std::mutex mutex_;
    int selected_user_ = -1;
    bool show_context_menu_ = false;
    int context_menu_x_ = 0;
    int context_menu_y_ = 0;
    ActionCallback on_action_;
};

} // namespace grotto::tui
```

**Step 2: Implement user view**

- Render user list with `●` bullet and username
- Render channel list with `#` prefix
- On right-click: capture mouse position, set `show_context_menu_ = true`, record which user was clicked
- Context menu renders as a floating `vbox` with options: Kick, Ban, Whois
- On menu selection: call `on_action_("kick", user_id)` etc.
- On click outside menu or Escape: close menu

**Step 3: Build and verify**

```bash
cmake --build build
```

**Step 4: Commit**

```bash
git commit -m "feat(tui): implement UserView with context menu (kick/ban/whois)"
```

---

## Task 7: Settings View Component

**Files:**
- Create: `grotto-server-tui/include/grotto/tui/settings_view.hpp`
- Modify: `grotto-server-tui/src/settings_view.cpp`

FTXUI component showing editable server settings. Hot-reload settings are editable; restart-required settings are read-only.

**Step 1: Define settings view**

```cpp
// include/grotto/tui/settings_view.hpp
#pragma once

#include <grotto/tui/admin_protocol.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace grotto::tui {

using ConfigChangeCallback = std::function<void(const std::string& key, const std::string& value)>;

class SettingsView {
public:
    explicit SettingsView(ConfigChangeCallback on_change);

    // Update config entries (thread-safe)
    void set_config(std::vector<protocol::ConfigEntry> entries);

    // FTXUI component
    ftxui::Component component();

    // FTXUI render
    ftxui::Element render();

private:
    std::vector<protocol::ConfigEntry> entries_;
    std::mutex mutex_;
    int selected_ = 0;
    bool editing_ = false;
    std::string edit_value_;
    ConfigChangeCallback on_change_;
};

} // namespace grotto::tui
```

**Step 2: Implement settings view**

- Render as a table: `[Key]  [Value]  [Status]`
- Hot-reload entries: highlighted, Enter opens inline edit
- Read-only entries: dimmed, show "(restart required)"
- On Enter while editing: call `on_change_(key, new_value)`
- Escape cancels edit

Hot-reload keys (from `ServerConfig`):
- `log_level`, `msg_rate_per_sec`, `commands_per_min`, `joins_per_min`
- `max_connections`, `abuse_threshold`, `ban_duration_min`

Read-only keys:
- `host`, `port`, `tls_cert_file`, `tls_key_file`, `db_path`

**Step 3: Build and verify**

```bash
cmake --build build
```

**Step 4: Commit**

```bash
git commit -m "feat(tui): implement SettingsView with hot-reload support"
```

---

## Task 8: Bug Report View Component

**Files:**
- Create: `grotto-server-tui/include/grotto/tui/bugreport_view.hpp`
- Modify: `grotto-server-tui/src/bugreport_view.cpp`

**Step 1: Define bug report view**

```cpp
// include/grotto/tui/bugreport_view.hpp
#pragma once

#include <grotto/tui/admin_protocol.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <mutex>
#include <vector>

namespace grotto::tui {

using BugReportActionCallback = std::function<void(int id, const std::string& new_status)>;

class BugReportView {
public:
    explicit BugReportView(BugReportActionCallback on_action);

    // Update bug reports (thread-safe)
    void set_reports(std::vector<protocol::BugReport> reports);

    // FTXUI component
    ftxui::Component component();

    // FTXUI render
    ftxui::Element render();

private:
    std::vector<protocol::BugReport> reports_;
    std::mutex mutex_;
    int selected_ = 0;
    bool show_detail_ = false;
    BugReportActionCallback on_action_;
};

} // namespace grotto::tui
```

**Step 2: Implement bug report view**

- List view: `[Status] [User] [Description (truncated)] [Date]`
- Status color: new=yellow, read=white, resolved=green
- Enter: toggle detail view (full description)
- Right-click or 's': cycle status (new -> read -> resolved)

**Step 3: Build and verify**

```bash
cmake --build build
```

**Step 4: Commit**

```bash
git commit -m "feat(tui): implement BugReportView component"
```

---

## Task 9: Main TUI Layout (AdminTui)

**Files:**
- Modify: `grotto-server-tui/include/grotto/tui/admin_tui.hpp`
- Modify: `grotto-server-tui/src/admin_tui.cpp`
- Modify: `grotto-server-tui/src/main.cpp`

Wire all components together into the final layout from the design doc.

**Step 1: Update AdminTui header**

```cpp
// include/grotto/tui/admin_tui.hpp
#pragma once

#include <grotto/tui/admin_socket_client.hpp>
#include <grotto/tui/log_view.hpp>
#include <grotto/tui/user_view.hpp>
#include <grotto/tui/settings_view.hpp>
#include <grotto/tui/bugreport_view.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <string>

namespace grotto::tui {

class AdminTui {
public:
    // Connect to server at given socket path (empty = default)
    explicit AdminTui(const std::string& socket_path = "");

    // Run the TUI (blocking — takes over terminal)
    int run();

private:
    void on_event(const nlohmann::json& event);
    void on_user_action(const std::string& action, const std::string& user_id);
    void on_config_change(const std::string& key, const std::string& value);
    void on_bugreport_action(int id, const std::string& status);
    void handle_command_input(const std::string& input);

    ftxui::Element build_layout();

    AdminSocketClient client_;
    ftxui::ScreenInteractive screen_;

    LogView log_view_;
    UserView user_view_;
    SettingsView settings_view_;
    BugReportView bugreport_view_;

    int active_tab_ = 0;           // 0=log, 1=settings, 2=bug reports
    std::string command_input_;     // Command line input text
    std::string status_text_;       // Status bar text
};

} // namespace grotto::tui
```

**Step 2: Implement AdminTui**

Key implementation points:

- **Constructor:** Create `AdminSocketClient`, set event callback, connect
- **`run()`:** Build FTXUI component tree, call `screen_.Loop()`
- **Layout:** Follow design doc exactly:
  - Top: tab bar `[F1 Loki] [F2 Asetukset] [F3 Bug Reports]` + version
  - Middle: 70/30 split — left=active tab view, right=user sidebar
  - Bottom: command input line `> ___`
- **Event handling:**
  - F1/F2/F3 switch `active_tab_`
  - Mouse events forwarded to active component
  - Enter on command input calls `handle_command_input()`
  - Ctrl+D = quit (detach)
- **`on_event()`:** Dispatch JSON events to correct view:
  - `"log"` → `log_view_.push()`
  - `"users"` → `user_view_.set_users()`
  - `"channels"` → `user_view_.set_channels()`
  - `"stats"` → update status bar
  - `"config"` → `settings_view_.set_config()`
  - `"bug_reports"` → `bugreport_view_.set_reports()`
  - After each event: `screen_.PostEvent(Event::Custom)` to trigger redraw
- **`handle_command_input()`:** Parse `/kick user reason`, `/ban user reason`, etc. Send as JSON command via client

**Step 3: Update main.cpp**

```cpp
// src/main.cpp
#include <grotto/tui/admin_tui.hpp>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string socket_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "grotto-tui - Admin TUI for Grotto Server\n\n"
                      << "Usage: grotto-tui [options]\n\n"
                      << "Options:\n"
                      << "  --socket <path>  Admin socket path (default: auto-detect)\n"
                      << "  --help, -h       Show this help\n";
            return 0;
        }
        if (arg == "--socket" && i + 1 < argc) {
            socket_path = argv[++i];
        }
    }

    grotto::tui::AdminTui tui(socket_path);
    return tui.run();
}
```

**Step 4: Build and verify**

```bash
cmake --build build
./build/grotto-tui --help
```

Expected: Shows help text. TUI won't fully run yet without a server, but should compile and show help.

**Step 5: Commit**

```bash
git commit -m "feat(tui): implement AdminTui main layout with all views wired together"
```

---

## Task 10: Server Integration

**Files:**
- Modify: `grotto-server/src/config.hpp` — add `headless` field to `ServerConfig`
- Modify: `grotto-server/src/main.cpp` — add `--headless` flag
- Modify: `grotto-server/src/server.hpp` — add `AdminSocketListener` member
- Modify: `grotto-server/src/server.cpp` — start admin socket listener, wire callbacks
- Modify: `grotto-server/CMakeLists.txt` — add `GROTTO_CHAT_SERVER_TUI` option + link

**Step 1: Add headless to ServerConfig**

In `grotto-server/src/config.hpp`, add to `ServerConfig`:

```cpp
bool headless = false;  // --headless: no TUI, admin socket only
```

**Step 2: Add --headless to argument parsing**

In `grotto-server/src/main.cpp`, add to `parse_command_line()`:

```cpp
if (arg == "--headless" || arg == "--daemon") {
    config_path_headless = true;  // store in a global, apply to config after load
}
```

And update the usage string to include `--headless`.

**Step 3: Add admin socket to server**

In `grotto-server/src/server.hpp`, add (guarded by `#ifdef GROTTO_CHAT_HAS_TUI`):

```cpp
#ifdef GROTTO_CHAT_HAS_TUI
#include <grotto/tui/admin_socket_listener.hpp>
#include <grotto/tui/admin_tui.hpp>
    std::shared_ptr<tui::AdminSocketListener> admin_listener_;
    std::unique_ptr<std::thread> tui_thread_;
#endif
```

**Step 4: Wire admin socket in server.cpp**

In `Server::run()`, after listener starts:

```cpp
#ifdef GROTTO_CHAT_HAS_TUI
    // Start admin socket listener
    admin_listener_ = std::make_shared<tui::AdminSocketListener>(ioc_);
    admin_listener_->set_command_callback([this](const nlohmann::json& cmd) {
        handle_admin_command(cmd);
    });
    admin_listener_->start();

    // Start TUI in integrated mode (unless headless)
    if (!config_.headless) {
        tui_thread_ = std::make_unique<std::thread>([this] {
            tui::AdminTui tui;  // connects to admin socket
            tui.run();
            // TUI exited (Ctrl+D) — server keeps running
        });
    }
#endif
```

Add `handle_admin_command()` method to process kick/ban/set_config commands by calling existing `Listener` methods.

Add a custom spdlog sink that forwards log entries to `admin_listener_->send_event()`.

Add periodic timer that sends `users`, `channels`, `stats` events every 1-2 seconds.

**Step 5: Update CMakeLists.txt**

In `grotto-server/CMakeLists.txt`:

```cmake
option(GROTTO_CHAT_SERVER_TUI "Build with TUI support" ON)
if(GROTTO_CHAT_SERVER_TUI)
    add_subdirectory(../grotto-server-tui ${CMAKE_BINARY_DIR}/grotto-server-tui)
    target_link_libraries(grotto-server PRIVATE grotto-server-tui)
    target_compile_definitions(grotto-server PRIVATE GROTTO_CHAT_HAS_TUI)
endif()
```

**Step 6: Build full server with TUI**

```bash
cd grotto-server
cmake -B build -DGROTTO_CHAT_SERVER_TUI=ON -DCMAKE_TOOLCHAIN_FILE=...
cmake --build build
```

**Step 7: Test both modes**

```bash
# Headless mode
./grotto-server --headless --config ./config/server.toml
# In another terminal:
./grotto-tui

# Integrated mode
./grotto-server --config ./config/server.toml
# TUI should appear directly
```

**Step 8: Commit**

```bash
git commit -m "feat: integrate TUI into grotto-server with --headless flag"
```

---

## Task 11: Custom spdlog Sink for TUI

**Files:**
- Create: `grotto-server-tui/include/grotto/tui/tui_log_sink.hpp`

A spdlog sink that forwards log entries to the `AdminSocketListener` as JSON events. This is what makes the real-time log view work.

**Step 1: Implement the sink**

```cpp
// include/grotto/tui/tui_log_sink.hpp
#pragma once

#include <grotto/tui/admin_socket_listener.hpp>
#include <spdlog/sinks/base_sink.h>
#include <chrono>
#include <memory>

namespace grotto::tui {

template<typename Mutex>
class TuiLogSink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit TuiLogSink(std::shared_ptr<AdminSocketListener> listener)
        : listener_(std::move(listener)) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (!listener_ || !listener_->has_client()) return;

        protocol::LogEntry entry;
        entry.level = std::string(spdlog::level::to_string_view(msg.level));
        entry.msg = std::string(msg.payload.begin(), msg.payload.end());

        // Format timestamp
        auto time = std::chrono::system_clock::to_time_t(msg.time);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time));
        entry.ts = buf;

        nlohmann::json j = entry;
        j["type"] = "log";
        listener_->send_event(j);
    }

    void flush_() override {}

private:
    std::shared_ptr<AdminSocketListener> listener_;
};

using TuiLogSinkMt = TuiLogSink<std::mutex>;

} // namespace grotto::tui
```

**Step 2: Register sink in server.cpp**

In `Server::run()`, after creating `admin_listener_`:

```cpp
auto tui_sink = std::make_shared<tui::TuiLogSinkMt>(admin_listener_);
tui_sink->set_level(spdlog::level::debug);
spdlog::default_logger()->sinks().push_back(tui_sink);
```

**Step 3: Build and verify**

```bash
cmake --build build
```

**Step 4: Commit**

```bash
git commit -m "feat(tui): add spdlog sink that forwards logs to TUI via admin socket"
```

---

## Task 12: End-to-End Testing & Polish

**Step 1: Manual test — headless + attach**

```bash
# Terminal 1: Start server headless
./grotto-server --headless

# Terminal 2: Attach TUI
./grotto-tui
# Verify: logs appear, users show in sidebar, tabs work
# Press Ctrl+D to detach
# Re-run grotto-tui to re-attach
```

**Step 2: Manual test — integrated mode**

```bash
./grotto-server
# Verify: TUI appears inline, shows logs, sidebar works
```

**Step 3: Test user management**

```bash
# In TUI command line:
> /kick TestUser spam
> /ban TestUser repeated spam
# Verify: commands reach server, user is disconnected
# Also test right-click context menu on user
```

**Step 4: Test settings hot-reload**

```bash
# In TUI: F2 -> select log_level -> Enter -> type "debug" -> Enter
# Verify: log output changes to include debug messages
```

**Step 5: Fix any issues found during testing**

**Step 6: Final commit**

```bash
git commit -m "feat(tui): end-to-end testing and polish"
```

---

## Dependency Graph

```
Task 1 (scaffold)
  └─► Task 2 (protocol)
       ├─► Task 3 (socket listener)
       └─► Task 4 (socket client)
            ├─► Task 5 (log view)
            ├─► Task 6 (user view)
            ├─► Task 7 (settings view)
            └─► Task 8 (bug report view)
                 └─► Task 9 (main layout — wires all views)
                      └─► Task 10 (server integration)
                           └─► Task 11 (spdlog sink)
                                └─► Task 12 (E2E testing)
```

**Parallelizable:** Tasks 3+4 can be done in parallel. Tasks 5+6+7+8 can be done in parallel (they share no state).
