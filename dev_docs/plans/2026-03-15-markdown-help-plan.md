# Markdown-renderöinti + Help-järjestelmä — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add markdown rendering to all messages in the FTXUI desktop client, and implement a `/help <topic>` command that loads help content from `.md` files on disk.

**Architecture:** New `markdown_renderer` module parses markdown text into FTXUI Elements. New `HelpManager` class caches `.md` files from `help/` directory relative to the binary. Both integrate into existing message rendering and command handling.

**Tech Stack:** C++20, FTXUI (dom/elements), `std::filesystem`

---

### Task 1: Markdown Renderer — Core Parser

**Files:**
- Create: `grotto-client/src/ui/markdown_renderer.hpp`
- Create: `grotto-client/src/ui/markdown_renderer.cpp`
- Modify: `grotto-client/CMakeLists.txt:178` (add source file)

**Step 1: Create header**

Create `grotto-client/src/ui/markdown_renderer.hpp`:

```cpp
#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>

namespace grotto::ui {

// Parse markdown text and return an FTXUI Element tree.
// Supports: **bold**, *italic*, `inline code`, ```code blocks```,
// # headings, - bullet lists, > blockquotes
ftxui::Element render_markdown(const std::string& text);

} // namespace grotto::ui
```

**Step 2: Create implementation**

Create `grotto-client/src/ui/markdown_renderer.cpp`:

```cpp
#include "ui/markdown_renderer.hpp"
#include "ui/color_scheme.hpp"
#include <ftxui/dom/elements.hpp>
#include <sstream>
#include <vector>
#include <string>

using namespace ftxui;

namespace grotto::ui {

namespace {

// Parse inline markdown elements within a single line.
// Returns an hbox of styled text segments.
Element parse_inline(const std::string& line) {
    Elements parts;
    size_t i = 0;
    std::string buf;

    auto flush_buf = [&]() {
        if (!buf.empty()) {
            parts.push_back(text(buf) | color(palette::fg()));
            buf.clear();
        }
    };

    while (i < line.size()) {
        // Bold: **text**
        if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '*') {
            flush_buf();
            size_t end = line.find("**", i + 2);
            if (end != std::string::npos) {
                parts.push_back(text(line.substr(i + 2, end - i - 2))
                                | bold | color(palette::fg()));
                i = end + 2;
                continue;
            }
        }
        // Italic: *text* (but not **)
        if (line[i] == '*' && (i + 1 >= line.size() || line[i + 1] != '*')) {
            flush_buf();
            size_t end = line.find('*', i + 1);
            if (end != std::string::npos) {
                parts.push_back(text(line.substr(i + 1, end - i - 1))
                                | dim | color(palette::fg()));
                i = end + 1;
                continue;
            }
        }
        // Inline code: `text`
        if (line[i] == '`') {
            flush_buf();
            size_t end = line.find('`', i + 1);
            if (end != std::string::npos) {
                parts.push_back(text(line.substr(i + 1, end - i - 1))
                                | inverted);
                i = end + 1;
                continue;
            }
        }
        buf += line[i];
        ++i;
    }
    flush_buf();

    if (parts.empty()) return text("");
    if (parts.size() == 1) return parts[0];
    return hbox(std::move(parts));
}

} // anonymous namespace

Element render_markdown(const std::string& md) {
    std::istringstream stream(md);
    std::string line;
    Elements blocks;
    bool in_code_block = false;
    Elements code_lines;

    while (std::getline(stream, line)) {
        // Code block fence: ```
        if (line.size() >= 3 && line.substr(0, 3) == "```") {
            if (in_code_block) {
                // End code block
                blocks.push_back(vbox(std::move(code_lines)) | inverted);
                code_lines.clear();
                in_code_block = false;
            } else {
                in_code_block = true;
            }
            continue;
        }

        if (in_code_block) {
            code_lines.push_back(text(line));
            continue;
        }

        // Heading: # text
        if (!line.empty() && line[0] == '#') {
            size_t level = 0;
            while (level < line.size() && line[level] == '#') ++level;
            std::string heading = line.substr(level);
            // Trim leading space
            if (!heading.empty() && heading[0] == ' ') heading = heading.substr(1);
            blocks.push_back(text(heading) | bold | underlined
                             | color(palette::fg()));
            continue;
        }

        // Blockquote: > text
        if (!line.empty() && line[0] == '>') {
            std::string content = line.substr(1);
            if (!content.empty() && content[0] == ' ') content = content.substr(1);
            blocks.push_back(hbox({
                text("│ ") | color(palette::comment()),
                parse_inline(content) | dim,
            }));
            continue;
        }

        // Bullet list: - text or * text (at start of line)
        if (line.size() >= 2 &&
            (line[0] == '-' || line[0] == '*') && line[1] == ' ') {
            // Disambiguate: * at start of line followed by space is a bullet,
            // not italic. Only treat as bullet if line starts with "- " or "* ".
            std::string content = line.substr(2);
            blocks.push_back(hbox({
                text("  • ") | color(palette::yellow()),
                parse_inline(content),
            }));
            continue;
        }

        // Normal line with inline parsing
        if (line.empty()) {
            blocks.push_back(text(""));
        } else {
            blocks.push_back(parse_inline(line));
        }
    }

    // Unclosed code block
    if (in_code_block && !code_lines.empty()) {
        blocks.push_back(vbox(std::move(code_lines)) | inverted);
    }

    if (blocks.empty()) return text("");
    return vbox(std::move(blocks));
}

} // namespace grotto::ui
```

**Step 3: Add to CMakeLists.txt**

In `grotto-client/CMakeLists.txt`, add `src/ui/markdown_renderer.cpp` to `CLIENT_SOURCES` (after line 183, `src/ui/user_list_panel.cpp`):

```cmake
    src/ui/markdown_renderer.cpp
```

**Step 4: Build and verify compilation**

Run:
```bash
cd grotto-client/build && cmake --build . --target grotto-client 2>&1 | tail -20
```
Expected: compiles without errors.

**Step 5: Commit**

```bash
git add grotto-client/src/ui/markdown_renderer.hpp grotto-client/src/ui/markdown_renderer.cpp grotto-client/CMakeLists.txt
git commit -m "feat: add markdown renderer for FTXUI"
```

---

### Task 2: Integrate Markdown into Message View

**Files:**
- Modify: `grotto-client/src/ui/message_view.cpp:27-43` (render_one function)

**Step 1: Add include**

Add at top of `message_view.cpp` (after existing includes):
```cpp
#include "ui/markdown_renderer.hpp"
```

**Step 2: Replace paragraph() with render_markdown()**

In `render_one()` at line 34, replace:
```cpp
            paragraph("* " + msg.content) | color(palette::yellow()) | flex,
```
with:
```cpp
            render_markdown("* " + msg.content) | color(palette::yellow()) | flex,
```

At line 41, replace:
```cpp
        paragraph(msg.content) | color(palette::fg()) | flex,
```
with:
```cpp
        render_markdown(msg.content) | flex,
```

**Step 3: Build and verify**

Run:
```bash
cd grotto-client/build && cmake --build . --target grotto-client 2>&1 | tail -20
```
Expected: compiles without errors.

**Step 4: Commit**

```bash
git add grotto-client/src/ui/message_view.cpp
git commit -m "feat: use markdown rendering for chat messages"
```

---

### Task 3: Help Manager

**Files:**
- Create: `grotto-client/src/help/help_manager.hpp`
- Create: `grotto-client/src/help/help_manager.cpp`
- Modify: `grotto-client/CMakeLists.txt` (add source file)

**Step 1: Create header**

Create `grotto-client/src/help/help_manager.hpp`:

```cpp
#pragma once
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace grotto {

class HelpManager {
public:
    // Construct with path to the directory containing the binary.
    explicit HelpManager(const std::filesystem::path& binary_dir);

    // Scan help/ directory and cache all .md files.
    void load();

    // Clear cache and reload from disk.
    void reload();

    // Return sorted list of available topic names (without .md extension).
    std::vector<std::string> topics() const;

    // Get cached content for a topic. Returns nullopt if not found.
    std::optional<std::string> get(const std::string& topic) const;

private:
    std::filesystem::path help_dir_;
    std::map<std::string, std::string> cache_;
};

} // namespace grotto
```

**Step 2: Create implementation**

Create `grotto-client/src/help/help_manager.cpp`:

```cpp
#include "help/help_manager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace grotto {

HelpManager::HelpManager(const std::filesystem::path& binary_dir)
    : help_dir_(binary_dir / "help") {}

void HelpManager::load() {
    cache_.clear();
    if (!std::filesystem::is_directory(help_dir_)) return;

    for (auto& entry : std::filesystem::directory_iterator(help_dir_)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".md") continue;

        std::string topic = entry.path().stem().string();
        std::ifstream f(entry.path());
        if (!f) continue;

        std::ostringstream ss;
        ss << f.rdbuf();
        cache_[topic] = ss.str();
    }
}

void HelpManager::reload() {
    load();
}

std::vector<std::string> HelpManager::topics() const {
    std::vector<std::string> result;
    result.reserve(cache_.size());
    for (auto& [k, _] : cache_) {
        result.push_back(k);
    }
    // std::map is already sorted, but be explicit
    std::sort(result.begin(), result.end());
    return result;
}

std::optional<std::string> HelpManager::get(const std::string& topic) const {
    // Sanitize: reject path traversal
    if (topic.find("..") != std::string::npos ||
        topic.find('/') != std::string::npos ||
        topic.find('\\') != std::string::npos) {
        return std::nullopt;
    }

    auto it = cache_.find(topic);
    if (it == cache_.end()) return std::nullopt;
    return it->second;
}

} // namespace grotto
```

**Step 3: Add to CMakeLists.txt**

Add `src/help/help_manager.cpp` to `CLIENT_SOURCES` (after `src/input/tab_complete.cpp`, line 200):

```cmake
    src/help/help_manager.cpp
```

**Step 4: Build and verify**

```bash
cd grotto-client/build && cmake --build . --target grotto-client 2>&1 | tail -20
```

**Step 5: Commit**

```bash
git add grotto-client/src/help/help_manager.hpp grotto-client/src/help/help_manager.cpp grotto-client/CMakeLists.txt
git commit -m "feat: add HelpManager for loading .md help files"
```

---

### Task 4: Wire Help Commands into App

**Files:**
- Modify: `grotto-client/src/app.hpp:12` (add include)
- Modify: `grotto-client/src/app.hpp:80` (add member)
- Modify: `grotto-client/src/app.cpp:395-403` (replace /help handler)
- Modify: `grotto-client/src/app.cpp:112` (init HelpManager in App::init)
- Modify: `grotto-client/src/input/command_parser.cpp:12` (add /reload_help)

**Step 1: Add HelpManager member to App**

In `app.hpp`, add include after line 12:
```cpp
#include "help/help_manager.hpp"
```

Add member after line 80 (`std::unique_ptr<LinkPreviewer> previewer_;`):
```cpp
    std::unique_ptr<HelpManager> help_;
```

**Step 2: Initialize HelpManager in App::init**

In `app.cpp`, inside `App::init()`, after existing initialization (find a suitable spot after logger init), add:
```cpp
    // Help system — resolve binary directory
    {
        auto bin_dir = std::filesystem::canonical(
            std::filesystem::path(config_path_).parent_path());
        help_ = std::make_unique<HelpManager>(bin_dir);
        help_->load();
    }
```

Note: `config_path_` points to `client.toml` which lives next to the binary, so its parent is the binary dir.

**Step 3: Replace /help handler in handle_command**

In `app.cpp`, replace lines 395-403 (the existing `/help` block):

```cpp
    if (cmd.name == "/help") {
        if (cmd.args.empty()) {
            auto content = help_ ? help_->get("help") : std::nullopt;
            if (content) {
                ui_->push_system_msg(*content);
            } else {
                // Fallback if help.md not found
                auto topics = help_ ? help_->topics() : std::vector<std::string>{};
                std::string list;
                for (auto& t : topics) { list += t + " "; }
                ui_->push_system_msg("Usage: /help <topic>");
                if (!list.empty())
                    ui_->push_system_msg("Available topics: " + list);
            }
        } else {
            // Sanitize topic name
            std::string topic = cmd.args[0];
            auto content = help_ ? help_->get(topic) : std::nullopt;
            if (content) {
                ui_->push_system_msg(*content);
            } else {
                ui_->push_system_msg("Topic '" + topic + "' not found.");
                auto topics = help_ ? help_->topics() : std::vector<std::string>{};
                std::string list;
                for (auto& t : topics) { list += t + " "; }
                if (!list.empty())
                    ui_->push_system_msg("Available topics: " + list);
            }
        }
        ui_->notify();
        return;
    }
```

**Step 4: Add /reload_help handler**

In `app.cpp`, add after the `/help` block:

```cpp
    if (cmd.name == "/reload_help") {
        if (help_) {
            help_->reload();
            ui_->push_system_msg("Help files reloaded.");
        } else {
            ui_->push_system_msg("Help system not initialized.");
        }
        ui_->notify();
        return;
    }
```

**Step 5: Add /reload_help to known commands**

In `command_parser.cpp`, add `/reload_help` to `kKnownCommands` (line 12, after `/help`):

```cpp
    "/quit", "/disconnect", "/status", "/help", "/reload_help", "/settings", "/version",
```

**Step 6: Build and verify**

```bash
cd grotto-client/build && cmake --build . --target grotto-client 2>&1 | tail -20
```

**Step 7: Commit**

```bash
git add grotto-client/src/app.hpp grotto-client/src/app.cpp grotto-client/src/input/command_parser.cpp
git commit -m "feat: wire /help <topic> and /reload_help commands"
```

---

### Task 5: Create Help Content Files

**Files:**
- Create: `grotto-client/help/help.md`
- Create: `grotto-client/help/commands.md`
- Create: `grotto-client/help/shortcuts.md`
- Create: `grotto-client/help/channels.md`
- Create: `grotto-client/help/voice.md`
- Create: `grotto-client/help/crypto.md`
- Create: `grotto-client/help/files.md`

**Step 1: Create help directory and files**

Create `grotto-client/help/help.md`:
```markdown
# Grotto Help

Welcome to **Grotto** — encrypted IRC-style messaging.

Type `/help <topic>` for detailed information:

- **commands** — All available commands
- **shortcuts** — Keyboard shortcuts
- **channels** — Joining and using channels
- **voice** — Voice chat features
- **crypto** — Encryption and trust
- **files** — File transfers
```

Create `grotto-client/help/commands.md`:
```markdown
# Commands

## Chat
- `/msg <user> <text>` — Send a direct message
- `/me <action>` — Send an action message

## Channels
- `/join <#channel>` — Join a channel
- `/part [#channel]` — Leave current or specified channel
- `/names` — List online users

## Voice
- `/call <user>` — Start a voice call
- `/accept` — Accept incoming call
- `/hangup` — End current call
- `/voice` — Toggle voice chat
- `/mute` — Toggle microphone mute
- `/deafen` — Toggle audio deafen
- `/ptt` — Toggle push-to-talk mode

## Security
- `/trust <user>` — Trust a user's identity key

## Utility
- `/search <query>` — Search message history
- `/clear` — Clear message view
- `/settings` — Open settings screen
- `/version` — Show client and server version
- `/status` — Show connection status
- `/help [topic]` — Show help
- `/reload_help` — Reload help files from disk

## Connection
- `/disconnect` — Disconnect from server
- `/quit` — Exit Grotto
```

Create `grotto-client/help/shortcuts.md`:
```markdown
# Keyboard Shortcuts

- **F1** — Toggle push-to-talk
- **F12** — Open settings
- **Alt+1-9** — Switch to channel by number
- **Alt+Left/Right** — Previous/next channel
- **Page Up/Down** — Scroll message history
- **Esc** — Quit
- **Tab** — Auto-complete commands and usernames
```

Create `grotto-client/help/channels.md`:
```markdown
# Channels

## Joining
Use `/join #channel-name` to join a channel. Channel names start with `#`.

## Leaving
Use `/part` to leave the current channel, or `/part #channel` to leave a specific one.

## Messaging
Simply type your message and press **Enter** to send to the active channel.

Switch between channels with **Alt+1-9** or **Alt+Left/Right**.

## User List
The right panel shows users in the current channel. Click the panel divider to resize.
```

Create `grotto-client/help/voice.md`:
```markdown
# Voice Chat

## Starting a Call
- `/call <user>` — Call another user
- `/accept` — Accept an incoming call

## During a Call
- `/mute` — Toggle your microphone
- `/deafen` — Mute all incoming audio
- `/ptt` — Switch to push-to-talk mode
- **F1** — Hold to talk (when PTT is active)
- `/hangup` — End the call

## Audio Settings
Open `/settings` to configure audio input/output devices.
```

Create `grotto-client/help/crypto.md`:
```markdown
# Encryption & Trust

Grotto uses **end-to-end encryption** for all messages. The server never sees plaintext.

## How It Works
- Each user generates an **Ed25519 identity key** on first launch
- Direct messages use the **Signal Protocol** (double ratchet)
- Channel messages use **Sender Keys** for group encryption

## Trusting Users
Use `/trust <user>` to mark a user's identity key as trusted. This verifies you are communicating with the right person.

## Key Management
Your identity key is stored locally. Use `--clear-creds` to reset it (you will need to re-establish sessions with all contacts).
```

Create `grotto-client/help/files.md`:
```markdown
# File Transfers

## Sending
Use `/upload <path>` to send a file to the current channel or user.

## Receiving
Use `/download` to save a received file. Files are end-to-end encrypted in transit.
```

**Step 2: Add CMake copy rule for help directory**

In `grotto-client/CMakeLists.txt`, after the existing `copy_if_different` commands (after line 253), add:

```cmake
# Copy help files to build output directory
add_custom_command(TARGET grotto-client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/help"
        "$<TARGET_FILE_DIR:grotto-client>/help"
    COMMENT "Copying help/ to output directory"
)
```

Also add `help` to the `EXTRA_FILES` in `add_release_zip_target` (line 273):
```cmake
    EXTRA_FILES README.md client.toml help
```

**Step 3: Build and verify**

```bash
cd grotto-client/build && cmake --build . --target grotto-client 2>&1 | tail -20
```

Verify help directory was copied:
```bash
ls grotto-client/build/Debug/help/ 2>/dev/null || ls grotto-client/build/help/ 2>/dev/null
```

**Step 4: Commit**

```bash
git add grotto-client/help/ grotto-client/CMakeLists.txt
git commit -m "feat: add help content files and CMake copy rule"
```

---

### Task 6: Manual Testing

**Step 1: Run the client and test**

Launch the client and test:
1. `/help` — should show the custom welcome text from `help.md`
2. `/help commands` — should show commands list with markdown formatting
3. `/help nonexistent` — should show "Topic not found" + available topics
4. `/reload_help` — should show "Help files reloaded."
5. Send a chat message with `**bold**` and `` `code` `` — should render with formatting

**Step 2: Update TODO & BUGS.md if needed**

Mark the help feature as done in the project docs.

**Step 3: Final commit if any fixes needed**

```bash
git add -A
git commit -m "feat: complete markdown + help system implementation"
```
