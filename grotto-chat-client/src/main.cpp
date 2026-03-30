#include "app.hpp"
#include <iostream>
#include <string>
#include <filesystem>
#include <optional>
#include <csignal>

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace {
#ifndef _WIN32
void swallow_interrupt_signal(int) {
}

void install_interrupt_handlers() {
    struct sigaction sa {};
    sa.sa_handler = swallow_interrupt_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGQUIT, &sa, nullptr);
}

class ScopedDisableTtyInterruptKeys {
public:
    ScopedDisableTtyInterruptKeys() {
        if (!isatty(STDIN_FILENO)) {
            return;
        }
        if (tcgetattr(STDIN_FILENO, &old_) != 0) {
            return;
        }
        termios t = old_;
        t.c_lflag &= ~ISIG;
#ifdef VINTR
        t.c_cc[VINTR] = _POSIX_VDISABLE;
#endif
#ifdef VQUIT
        t.c_cc[VQUIT] = _POSIX_VDISABLE;
#endif
        if (tcsetattr(STDIN_FILENO, TCSANOW, &t) == 0) {
            active_ = true;
        }
    }

    ~ScopedDisableTtyInterruptKeys() {
        if (active_) {
            tcsetattr(STDIN_FILENO, TCSANOW, &old_);
        }
    }

private:
    bool active_ = false;
    termios old_{};
};
#endif
} // namespace

// Parse ircord:// URL and return host:port
// Format: ircord://host:port or ircord://host (default port 6697)
static std::optional<std::pair<std::string, uint16_t>> parse_grotto_url(const std::string& url) {
    const std::string prefix = "ircord://";
    if (url.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }
    
    std::string rest = url.substr(prefix.length());
    
    // Remove any trailing slash or path
    size_t slash_pos = rest.find('/');
    if (slash_pos != std::string::npos) {
        rest = rest.substr(0, slash_pos);
    }
    
    // Parse host and port
    size_t colon_pos = rest.find(':');
    if (colon_pos == std::string::npos) {
        // No port specified, use default
        return std::make_pair(rest, uint16_t(6697));
    }
    
    std::string host = rest.substr(0, colon_pos);
    try {
        int port = std::stoi(rest.substr(colon_pos + 1));
        if (port < 1 || port > 65535) {
            return std::nullopt;
        }
        return std::make_pair(host, uint16_t(port));
    } catch (...) {
        return std::nullopt;
    }
}

static void print_help(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [OPTIONS] [ircord://host:port]\n"
              << "\n"
              << "Options:\n"
              << "  --config <path>   Path to client.toml (default: platform config dir)\n"
              << "  --user   <id>     Override user_id from config\n"
              << "  --clear-creds     Clear remembered credentials and local encrypted identity\n"
              << "  --help            Show this help\n"
              << "\n"
              << "Examples:\n"
              << "  " << argv0 << "\n"
              << "  " << argv0 << " ircord://chat.example.com:6697\n"
              << "  " << argv0 << " ircord://localhost:6667\n";
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Prevent Windows Console from killing the process on Ctrl+C and make
    // Ctrl+C arrive as a normal key event instead of a processed console
    // signal. This is needed because different terminals/SSH hops may still
    // short-circuit the older handler-only approach.
    SetConsoleCtrlHandler([](DWORD ctrl_type) -> BOOL {
        return (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT) ? TRUE : FALSE;
    }, TRUE);

    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (hin != INVALID_HANDLE_VALUE && GetConsoleMode(hin, &mode)) {
        SetConsoleMode(hin, mode & ~ENABLE_PROCESSED_INPUT);
    }
#else
    // Swallow SIGINT/SIGQUIT globally so Ctrl+C won't terminate the process.
    install_interrupt_handlers();
    ScopedDisableTtyInterruptKeys scoped_tty_interrupt_keys;
#endif

    std::filesystem::path config_path;
    std::string           user_id_override;
    bool                  clear_creds = false;
    std::optional<std::pair<std::string, uint16_t>> server_url;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help(argv[0]);
            return 0;
        } else if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            config_path = argv[++i];
        } else if ((arg == "--user" || arg == "-u") && i + 1 < argc) {
            user_id_override = argv[++i];
        } else if (arg == "--clear-creds") {
            clear_creds = true;
        } else if (arg.rfind("ircord://", 0) == 0) {
            // Parse ircord:// URL
            auto parsed = parse_grotto_url(arg);
            if (parsed) {
                server_url = parsed;
            } else {
                std::cerr << "Invalid Grotto URL: " << arg << "\n";
                std::cerr << "Expected format: ircord://host:port or ircord://host\n";
                return 1;
            }
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_help(argv[0]);
            return 1;
        }
    }

    // Default config path
    if (config_path.empty()) {

        // check if app path has client.toml
       
        if ( std::filesystem::exists( "./client.toml" )) {
            config_path = "./client.toml";
        }
        else {
            config_path = grotto::default_config_dir() / "client.toml";
        }
    }

    if (clear_creds) {
        auto cfg = grotto::load_config(config_path);
        std::string status_message;
        bool ok = grotto::clear_local_client_state(cfg, config_path, &status_message);
        if (!status_message.empty()) {
            std::cout << status_message << "\n";
        }
        return ok ? 0 : 1;
    }

    grotto::App app;
    if (!app.init(config_path, user_id_override, server_url)) {
        std::cerr << "Application initialization failed.\n";
        return 1;
    }

    return app.run();
}
