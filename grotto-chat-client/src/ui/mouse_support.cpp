#include "ui/mouse_support.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <regex>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace grotto::ui {

namespace {

enum class LinuxClipboardBackend {
    Unknown,
    WlCopy,
    Xclip,
    Osc52,
};

LinuxClipboardBackend g_linux_clipboard_backend = LinuxClipboardBackend::Unknown;
bool g_linux_clipboard_initialized = false;

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

std::optional<std::string> read_pipe_output(const char* command) {
    FILE* pipe = popen(command, "r");
    if (!pipe) {
        return std::nullopt;
    }

    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }

    pclose(pipe);
    if (output.empty()) {
        return std::nullopt;
    }
    return output;
}

std::string base64_encode(std::string_view input) {
    static constexpr char kBase64Alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= input.size()) {
        const uint32_t chunk = (static_cast<uint32_t>(static_cast<unsigned char>(input[i])) << 16) |
                               (static_cast<uint32_t>(static_cast<unsigned char>(input[i + 1])) << 8) |
                               static_cast<uint32_t>(static_cast<unsigned char>(input[i + 2]));
        out.push_back(kBase64Alphabet[(chunk >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(chunk >> 12) & 0x3F]);
        out.push_back(kBase64Alphabet[(chunk >> 6) & 0x3F]);
        out.push_back(kBase64Alphabet[chunk & 0x3F]);
        i += 3;
    }

    const size_t rem = input.size() - i;
    if (rem == 1) {
        const uint32_t chunk = static_cast<uint32_t>(static_cast<unsigned char>(input[i])) << 16;
        out.push_back(kBase64Alphabet[(chunk >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(chunk >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        const uint32_t chunk =
            (static_cast<uint32_t>(static_cast<unsigned char>(input[i])) << 16) |
            (static_cast<uint32_t>(static_cast<unsigned char>(input[i + 1])) << 8);
        out.push_back(kBase64Alphabet[(chunk >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(chunk >> 12) & 0x3F]);
        out.push_back(kBase64Alphabet[(chunk >> 6) & 0x3F]);
        out.push_back('=');
    }

    return out;
}

void copy_via_osc52(std::string_view text) {
    if (text.empty()) {
        return;
    }

    const std::string encoded = base64_encode(text);
    // Plain OSC52 (BEL and ST terminators).
    std::cout << "\033]52;c;" << encoded << "\a";
    std::cout << "\033]52;c;" << encoded << "\033\\";

    // tmux wrapped OSC52.
    if (std::getenv("TMUX")) {
        std::cout << "\033Ptmux;\033\033]52;c;" << encoded << "\a\033\\";
    }

    // GNU screen style DCS passthrough.
    if (std::getenv("STY")) {
        std::cout << "\033P\033]52;c;" << encoded << "\a\033\\";
    }

    std::cout << std::flush;
}

bool copy_with_command_and_verify(const std::string& text,
                                  const char* write_cmd,
                                  const char* read_cmd) {
    FILE* pipe = popen(write_cmd, "w");
    if (!pipe) {
        return false;
    }

    const size_t written = fwrite(text.c_str(), 1, text.size(), pipe);
    const int close_rc = pclose(pipe);
    if (written != text.size() || close_rc != 0) {
        return false;
    }

    auto read_back = read_pipe_output(read_cmd);
    if (!read_back) {
        return false;
    }
    return *read_back == text;
}

bool write_with_command(const std::string& text, const char* write_cmd) {
    FILE* pipe = popen(write_cmd, "w");
    if (!pipe) {
        return false;
    }
    const size_t written = fwrite(text.c_str(), 1, text.size(), pipe);
    const int close_rc = pclose(pipe);
    return written == text.size() && close_rc == 0;
}

std::string make_probe_token() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return "__grotto_clip_probe_" + std::to_string(now) + "__";
}

void probe_linux_clipboard_backend() {
    if (g_linux_clipboard_initialized) {
        return;
    }
    g_linux_clipboard_initialized = true;

    // Best effort preserve old clipboard contents from available readers.
    std::optional<std::string> old_wl = read_pipe_output("wl-paste -n 2>/dev/null");
    std::optional<std::string> old_xclip = read_pipe_output("xclip -selection clipboard -o 2>/dev/null");
    const std::string token = make_probe_token();

    if (copy_with_command_and_verify(token, "wl-copy 2>/dev/null", "wl-paste -n 2>/dev/null")) {
        g_linux_clipboard_backend = LinuxClipboardBackend::WlCopy;
        if (old_wl) {
            (void)write_with_command(*old_wl, "wl-copy 2>/dev/null");
        } else if (old_xclip) {
            (void)write_with_command(*old_xclip, "wl-copy 2>/dev/null");
        }
        return;
    }

    if (copy_with_command_and_verify(token,
                                     "xclip -selection clipboard 2>/dev/null",
                                     "xclip -selection clipboard -o 2>/dev/null")) {
        g_linux_clipboard_backend = LinuxClipboardBackend::Xclip;
        if (old_xclip) {
            (void)write_with_command(*old_xclip, "xclip -selection clipboard 2>/dev/null");
        } else if (old_wl) {
            (void)write_with_command(*old_wl, "xclip -selection clipboard 2>/dev/null");
        }
        return;
    }

    g_linux_clipboard_backend = LinuxClipboardBackend::Osc52;
}

#ifdef _WIN32
std::wstring utf8_to_utf16(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                       static_cast<int>(text.size()), nullptr, 0);
    if (wide_len <= 0) {
        return {};
    }

    std::wstring wide(static_cast<size_t>(wide_len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        wide.data(), wide_len);
    return wide;
}

std::string utf16_to_utf8(const wchar_t* text) {
    if (!text || *text == L'\0') {
        return {};
    }

    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (utf8_len <= 1) {
        return {};
    }

    std::string utf8(static_cast<size_t>(utf8_len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8.data(), utf8_len, nullptr, nullptr);
    utf8.pop_back();
    return utf8;
}
#endif

} // namespace

// ============================================================================
// MouseTracker Implementation
// ============================================================================

void initialize_clipboard_backend() {
#if !defined(_WIN32) && !defined(__APPLE__)
    probe_linux_clipboard_backend();
#endif
}

void MouseTracker::record_click(int x, int y) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_click_time_).count();
    
    int dx = std::abs(x - last_click_x_);
    int dy = std::abs(y - last_click_y_);
    
    if (elapsed < MouseConfig::kDoubleClickThresholdMs && 
        dx <= MouseConfig::kClickDistanceThreshold && 
        dy <= MouseConfig::kClickDistanceThreshold) {
        click_count_++;
    } else {
        click_count_ = 1;
    }
    
    last_click_time_ = now;
    last_click_x_ = x;
    last_click_y_ = y;
    x_ = x;
    y_ = y;
}

bool MouseTracker::is_double_click() const {
    return click_count_ == 2;
}

bool MouseTracker::is_triple_click() const {
    return click_count_ >= 3;
}

void MouseTracker::start_drag(int x, int y) {
    is_dragging_ = true;
    drag_start_x_ = x;
    drag_start_y_ = y;
}

void MouseTracker::update_drag(int x, int y) {
    x_ = x;
    y_ = y;
}

void MouseTracker::end_drag() {
    is_dragging_ = false;
}

void MouseTracker::start_selection(int x, int y) {
    is_selecting_ = true;
    selection_start_x_ = x;
    selection_start_y_ = y;
    selection_end_x_ = x;
    selection_end_y_ = y;
}

void MouseTracker::update_selection(int x, int y) {
    selection_end_x_ = x;
    selection_end_y_ = y;
}

void MouseTracker::end_selection() {
    is_selecting_ = false;
}

UIRegion MouseTracker::selection_region() const {
    int x1 = std::min(selection_start_x_, selection_end_x_);
    int y1 = std::min(selection_start_y_, selection_end_y_);
    int x2 = std::max(selection_start_x_, selection_end_x_);
    int y2 = std::max(selection_start_y_, selection_end_y_);
    
    return {x1, y1, x2 - x1 + 1, y2 - y1 + 1};
}

// ============================================================================
// Clipboard Implementation
// ============================================================================

void copy_to_clipboard(const std::string& text) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return;

    EmptyClipboard();

    std::wstring wide = utf8_to_utf16(text);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE,
        (wide.size() + 1) * sizeof(wchar_t));
    if (hMem) {
        auto* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
        if (pMem) {
            std::copy(wide.begin(), wide.end(), pMem);
            pMem[wide.size()] = L'\0';
            GlobalUnlock(hMem);
            if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
                GlobalFree(hMem);
            }
        } else {
            GlobalFree(hMem);
        }
    }

    CloseClipboard();
#elif __APPLE__
    // macOS - use pbcopy
    FILE* pipe = popen("pbcopy", "w");
    if (pipe) {
        fwrite(text.c_str(), 1, text.size(), pipe);
        pclose(pipe);
    }
#else
    probe_linux_clipboard_backend();
    if (g_linux_clipboard_backend == LinuxClipboardBackend::WlCopy) {
        if (!write_with_command(text, "wl-copy 2>/dev/null")) {
            copy_via_osc52(text);
        }
        return;
    }
    if (g_linux_clipboard_backend == LinuxClipboardBackend::Xclip) {
        if (!write_with_command(text, "xclip -selection clipboard 2>/dev/null")) {
            copy_via_osc52(text);
        }
        return;
    }
    copy_via_osc52(text);
#endif
}

std::optional<std::string> read_from_clipboard() {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) {
        return std::nullopt;
    }

    std::optional<std::string> result;

    if (HANDLE handle = GetClipboardData(CF_UNICODETEXT)) {
        if (auto* text = static_cast<const wchar_t*>(GlobalLock(handle))) {
            result = utf16_to_utf8(text);
            GlobalUnlock(handle);
        }
    } else if (HANDLE handle = GetClipboardData(CF_TEXT)) {
        if (auto* text = static_cast<const char*>(GlobalLock(handle))) {
            result = std::string(text);
            GlobalUnlock(handle);
        }
    }

    CloseClipboard();
    return result;
#elif __APPLE__
    return read_pipe_output("pbpaste");
#else
    if (auto text = read_pipe_output("wl-paste -n 2>/dev/null")) {
        return text;
    }
    return read_pipe_output("xclip -selection clipboard -o 2>/dev/null");
#endif
}

// ============================================================================
// URL Detection and Opening
// ============================================================================

bool is_url(const std::string& text) {
    static const std::regex url_regex(
        R"((https?://|www\.)[a-zA-Z0-9\-\.]+\.[a-zA-Z]{2,}(/\S*)?)",
        std::regex::icase
    );
    return std::regex_search(text, url_regex);
}

void open_url(const std::string& url) {
#ifdef _WIN32
    // Windows: ShellExecuteA is non-blocking and works from console apps
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif __APPLE__
    std::system(("open \"" + url + "\" 2>/dev/null &").c_str());
#else
    std::system(("xdg-open \"" + url + "\" 2>/dev/null || "
                 "firefox \"" + url + "\" 2>/dev/null || "
                 "chromium \"" + url + "\" 2>/dev/null &").c_str());
#endif
}

} // namespace grotto::ui
