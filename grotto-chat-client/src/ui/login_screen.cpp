#include "ui/login_screen.hpp"
#include "ui/color_scheme.hpp"
#include "version.hpp"
#include "i18n/strings.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace ftxui;

namespace grotto::ui {

namespace {

constexpr int kMinLoginWidth = 95;
constexpr int kMinLoginHeight = 40;

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
#endif

// Simple XOR encryption for remembered credentials (obfuscation, not high security)
// In production, this should use proper keychain/os credential APIs
std::string encrypt_simple(const std::string& data, const std::string& key) {
    std::string result;
    result.reserve(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        result.push_back(data[i] ^ key[i % key.size()]);
    }
    return result;
}

std::string decrypt_simple(const std::string& data, const std::string& key) {
    return encrypt_simple(data, key); // XOR is symmetric
}

// Generate a simple machine-specific key
std::string get_machine_key() {
    // Use a combination of machine-specific values
    std::string key = "grotto_v1_";
#ifdef _WIN32
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile) key += userprofile;
#else
    const char* home = std::getenv("HOME");
    if (home) key += home;
#endif
    // Pad or truncate to 32 chars
    if (key.size() < 32) {
        key.append(32 - key.size(), '0');
    } else if (key.size() > 32) {
        key = key.substr(0, 32);
    }
    return key;
}

} // anonymous namespace

LoginScreen::LoginScreen() = default;

LoginResult LoginScreen::show(const ClientConfig& existing_cfg,
                              ftxui::ScreenInteractive& screen,
                              LoginCredentials& out_creds,
                              const std::optional<LoginCredentials>& prefill,
                              const std::string& initial_status,
                              const std::string& initial_status_hint,
                              bool initial_status_is_error) {
    submitted_ = false;
    cancelled_ = false;
    clear_local_data_ = false;
    is_loading_ = false;
    status_message_ = initial_status;
    status_hint_ = initial_status_hint;
    status_is_error_ = initial_status_is_error;

    // Pre-fill with existing config values
    host_ = existing_cfg.server.host;
    if (host_.empty() || host_ == "localhost") {
        host_ = "chat.rausku.com"; // Default suggested host
    }
    port_str_ = std::to_string(existing_cfg.server.port);
    if (port_str_ == "6667") {
        port_str_ = "6697"; // Default secure port
    }
    username_ = existing_cfg.identity.user_id;
    if (username_ == "user") {
        username_.clear();
    }

    // Try to load remembered credentials
    try {
        auto creds_path = credentials_path_for_config_dir(existing_cfg.config_dir);
        if (std::filesystem::exists(creds_path)) {
            std::ifstream file(creds_path, std::ios::binary);
            if (file) {
                std::string encrypted((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());
                std::string decrypted = decrypt_simple(encrypted, get_machine_key());
                
                // Parse format: host\nport\nusername\npasskey
                size_t pos = 0;
                std::vector<std::string> parts;
                while (pos < decrypted.size()) {
                    size_t end = decrypted.find('\n', pos);
                    if (end == std::string::npos) end = decrypted.size();
                    parts.push_back(decrypted.substr(pos, end - pos));
                    pos = end + 1;
                }
                
                if (parts.size() >= 4) {
                    host_ = parts[0];
                    port_str_ = parts[1];
                    username_ = parts[2];
                    passkey_ = parts[3];
                    remember_ = true;
                }
            }
        }
    } catch (...) {
        // Ignore load errors
    }

    if (prefill) {
        if (!prefill->host.empty()) {
            host_ = prefill->host;
        }
        if (prefill->port != 0) {
            port_str_ = std::to_string(prefill->port);
        }
        if (!prefill->username.empty()) {
            username_ = prefill->username;
        }
        if (!prefill->passkey.empty()) {
            passkey_ = prefill->passkey;
        }
        remember_ = remember_ || prefill->remember;
    }

    // Get exit closure before building UI
#ifndef _WIN32
    install_interrupt_handlers();
#endif
    screen.ForceHandleCtrlC(false);
    auto exit_closure = screen.ExitLoopClosure();

    ftxui::InputOption host_option;
    host_option.content = &host_;
    host_option.placeholder = "chat.rausku.com";
    host_option.multiline = false;

    ftxui::InputOption port_option;
    port_option.content = &port_str_;
    port_option.placeholder = "6697";
    port_option.multiline = false;

    ftxui::InputOption username_option;
    username_option.content = &username_;
    username_option.placeholder = "username";
    username_option.multiline = false;

    bool passkey_masked = true;
    ftxui::InputOption passkey_option;
    passkey_option.content = &passkey_;
    passkey_option.placeholder = "passkey";
    passkey_option.multiline = false;
    passkey_option.password = &passkey_masked;

    host_input_ = Input(host_option);
    port_input_ = Input(port_option);
    username_input_ = Input(username_option);
    passkey_input_ = Input(passkey_option);
    remember_checkbox_ = Checkbox(i18n::tr(i18n::I18nKey::REMEMBER_CREDENTIALS), &remember_);

    // Connect button callback - exits the loop on valid submit
    connect_button_ = Button(i18n::tr(i18n::I18nKey::BUTTON_CONNECT), [this, exit_closure] {
        if (validate_inputs()) {
            submitted_ = true;
            exit_closure();
        }
    });
    clear_button_ = Button(i18n::tr(i18n::I18nKey::BUTTON_CLEAR_CREDS), [this, exit_closure] {
        clear_local_data_ = true;
        exit_closure();
    });
    quit_button_ = Button(i18n::tr(i18n::I18nKey::BUTTON_QUIT), [this, exit_closure] {
        cancelled_ = true;
        exit_closure();
    });

    // Update container with new button
    container_ = Container::Vertical({
        host_input_,
        port_input_,
        username_input_,
        passkey_input_,
        remember_checkbox_,
        clear_button_,
        connect_button_,
        quit_button_,
    });

    // Build the UI with event handling
    auto base_renderer = Renderer(container_, [this, &screen] {
        const int term_width = screen.dimx() > 0 ? screen.dimx() : 80;
        const int term_height = screen.dimy() > 0 ? screen.dimy() : 24;
        if (term_width < kMinLoginWidth || term_height < kMinLoginHeight) {
            auto warning_box = vbox({
                text(i18n::tr(i18n::I18nKey::LOGIN_MIN_SIZE_TITLE)) | bold | center,
                text(""),
                paragraphAlignCenter(i18n::tr(
                    i18n::I18nKey::LOGIN_MIN_SIZE_BODY,
                    std::vector<std::string>{
                        std::to_string(kMinLoginWidth),
                        std::to_string(kMinLoginHeight),
                        std::to_string(term_width),
                        std::to_string(term_height),
                    })) | center,
                text(""),
                paragraphAlignCenter(i18n::tr(i18n::I18nKey::LOGIN_MIN_SIZE_HINT)) |
                    color(palette::comment()) | dim | center,
            }) | size(WIDTH, LESS_THAN, 42) | border;

            return vbox({
                filler(),
                text(std::string("Grotto v. ") + std::string(grotto::VERSION)) |
                    bold | color(palette::blue()) | center,
                text(""),
                warning_box | center,
                text(""),
                text(i18n::tr(i18n::I18nKey::BUTTON_QUIT) + " / Esc") |
                    color(palette::comment()) | dim | center,
                filler(),
            }) | bgcolor(palette::bg()) | color(palette::fg());
        }
        const bool narrow_layout = term_width < 90;
        const bool compact_layout = term_height < 24;
        const bool minimal_layout = term_height < 20;
        const int form_width = std::clamp(term_width - (narrow_layout ? 2 : 8), 42, 66);
        const int input_width = narrow_layout
            ? std::clamp(form_width - 4, 18, 42)
            : std::clamp(form_width - 16, 18, 42);
        const auto section_title = [](const std::string& title) {
            return hbox({
                text(" " + title + " ") | bold | color(palette::blue()),
                filler(),
            });
        };
        const auto input_row = [&](const std::string& label, Component input) {
            auto field = input->Render() | size(WIDTH, EQUAL, input_width) | border;
            if (narrow_layout) {
                return vbox({
                    text(label) | color(palette::comment()),
                    field,
                });
            }
            return hbox({
                text(label) | color(palette::comment()),
                field,
            });
        };

        // Title
        auto title = text(std::string("Grotto v. ") + std::string(grotto::VERSION)) |
                     bold | color(palette::blue()) | center;

        Element buttons;
        if (form_width < 56) {
            buttons = vbox({
                remember_checkbox_->Render(),
                text(""),
                quit_button_->Render() | hcenter,
                clear_button_->Render() | hcenter,
                connect_button_->Render() | (is_loading_ ? dim : nothing) | hcenter,
            });
        } else if (narrow_layout) {
            buttons = vbox({
                remember_checkbox_->Render(),
                text(""),
                hbox({
                    quit_button_->Render(),
                    text(" "),
                    clear_button_->Render(),
                    text(" "),
                    connect_button_->Render() | (is_loading_ ? dim : nothing),
                }) | hcenter,
            });
        } else {
            buttons = hbox({
                remember_checkbox_->Render(),
                filler(),
                quit_button_->Render(),
                text(" "),
                clear_button_->Render(),
                text(" "),
                connect_button_->Render() | (is_loading_ ? dim : nothing),
            });
        }

        // Build the form
        Elements form_rows{
            section_title(i18n::tr(i18n::I18nKey::LOGIN_SERVER_SECTION)),
            separator(),
            input_row(i18n::tr(i18n::I18nKey::HOST_LABEL), host_input_),
            text(""),
            input_row(i18n::tr(i18n::I18nKey::PORT_LABEL), port_input_),
            separator(),
            section_title(i18n::tr(i18n::I18nKey::LOGIN_IDENTITY_SECTION)),
            separator(),
            input_row(i18n::tr(i18n::I18nKey::USERNAME_LABEL), username_input_),
            text(""),
            input_row(i18n::tr(i18n::I18nKey::PASSKEY_LABEL), passkey_input_),
            text(""),
            buttons,
        };
        if (!minimal_layout) {
            form_rows.push_back(text(""));
            form_rows.push_back(
                paragraphAlignLeft(i18n::tr(i18n::I18nKey::LOGIN_HELP_HINT)) |
                color(palette::comment()) | dim);
        }
        if (!compact_layout) {
            form_rows.push_back(text(""));
            form_rows.push_back(
                paragraphAlignLeft(i18n::tr(i18n::I18nKey::LOGIN_CLEAR_CREDS_HINT)) |
                color(palette::comment()) | dim);
        }

        auto form = vbox(std::move(form_rows)) | size(WIDTH, EQUAL, form_width) | border | center;

        Element status_el;
        if (!status_message_.empty()) {
            Elements status_rows{
                text(i18n::tr(status_is_error_
                              ? i18n::I18nKey::LOGIN_STATUS_ERROR_TITLE
                              : i18n::I18nKey::LOGIN_STATUS_INFO_TITLE)) |
                    bold | color(status_is_error_ ? palette::error_c() : palette::cyan()),
                text(""),
                paragraphAlignLeft(status_message_),
            };
            if (!status_hint_.empty()) {
                status_rows.push_back(text(""));
                status_rows.push_back(
                    paragraphAlignLeft(status_hint_) |
                    color(palette::comment()) | dim);
            }
            status_el = vbox(std::move(status_rows)) |
                        size(WIDTH, EQUAL, form_width) |
                        border;
        } else {
            status_el = text("") | center;
        }

        // Loading indicator
        Element loading_el;
        if (is_loading_) {
            loading_el = text(i18n::tr(i18n::I18nKey::CONNECTING)) | color(palette::cyan()) | blink | center;
        } else {
            loading_el = text("") | center;
        }

        // Main layout - centered form with title and status
        Elements root_rows;
        if (!compact_layout) {
            root_rows.push_back(filler());
        }
        root_rows.push_back(title);
        root_rows.push_back(text("") | size(HEIGHT, EQUAL, compact_layout ? 0 : 1));
        root_rows.push_back(form);
        root_rows.push_back(text("") | size(HEIGHT, EQUAL, 1));
        root_rows.push_back(status_el);
        root_rows.push_back(loading_el);
        if (!compact_layout) {
            root_rows.push_back(filler());
        }
        return vbox(std::move(root_rows)) | bgcolor(palette::bg()) | color(palette::fg());
    });

    // Event handler for Escape and Enter
    auto component = CatchEvent(base_renderer, [this, exit_closure](Event event) -> bool {
        if (event == Event::CtrlC || event.input() == "\x03" || event.input() == "\x04") {
            return true;
        }
        if (event == Event::Escape) {
            cancelled_ = true;
            exit_closure();
            return true;
        }
        if (event == Event::Return && !is_loading_) {
            // Enter key submits the form
            if (validate_inputs()) {
                submitted_ = true;
                exit_closure();
                return true;
            }
        }
        return false;
    });

    // Run the login screen
    screen.Loop(component);

    if (cancelled_) {
        return LoginResult::Cancelled;
    }

    if (clear_local_data_) {
        return LoginResult::ClearLocalData;
    }

    if (submitted_ && validate_inputs()) {
        // Fill output credentials
        out_creds.host = host_;
        out_creds.port = static_cast<uint16_t>(std::atoi(port_str_.c_str()));
        out_creds.username = username_;
        out_creds.passkey = passkey_;
        out_creds.remember = remember_;

        // Save credentials if remember is checked
        if (remember_) {
            try {
                auto creds_path = credentials_path_for_config_dir(existing_cfg.config_dir);
                std::filesystem::create_directories(creds_path.parent_path());
                
                std::string data = host_ + "\n" + port_str_ + "\n" + username_ + "\n" + passkey_;
                std::string encrypted = encrypt_simple(data, get_machine_key());
                
                std::ofstream file(creds_path, std::ios::binary);
                if (file) {
                    file.write(encrypted.data(), encrypted.size());
                }
            } catch (const std::exception& e) {
                spdlog::warn("Failed to save credentials: {}", e.what());
            }
        } else {
            // Delete saved credentials if remember is unchecked
            try {
                auto creds_path = credentials_path_for_config_dir(existing_cfg.config_dir);
                if (std::filesystem::exists(creds_path)) {
                    std::filesystem::remove(creds_path);
                }
            } catch (...) {
                // Ignore deletion errors
            }
        }

        return LoginResult::Success;
    }

    return LoginResult::Cancelled;
}

void LoginScreen::set_error(const std::string& error) {
    status_message_ = error;
    status_hint_.clear();
    status_is_error_ = true;
}

void LoginScreen::set_loading(bool loading) {
    is_loading_ = loading;
}

// build_ui() implementation is inlined in show() for proper exit handling

bool LoginScreen::validate_inputs() {
    status_message_.clear();
    status_hint_.clear();
    status_is_error_ = true;

    if (host_.empty()) {
        status_message_ = i18n::tr(i18n::I18nKey::HOST_REQUIRED);
        return false;
    }

    if (port_str_.empty()) {
        status_message_ = i18n::tr(i18n::I18nKey::PORT_REQUIRED);
        return false;
    }

    // Validate port is a number
    try {
        int port = std::atoi(port_str_.c_str());
        if (port <= 0 || port > 65535) {
            status_message_ = i18n::tr(i18n::I18nKey::PORT_RANGE_ERROR);
            return false;
        }
    } catch (...) {
        status_message_ = i18n::tr(i18n::I18nKey::PORT_NUMBER_ERROR);
        return false;
    }

    if (username_.empty()) {
        status_message_ = i18n::tr(i18n::I18nKey::USERNAME_REQUIRED);
        return false;
    }

    if (passkey_.empty()) {
        status_message_ = i18n::tr(i18n::I18nKey::PASSKEY_REQUIRED);
        return false;
    }

    return true;
}

} // namespace grotto::ui
