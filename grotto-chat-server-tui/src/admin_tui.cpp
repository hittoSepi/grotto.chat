#include <grotto/tui/admin_tui.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <spdlog/spdlog.h>

#include <sstream>

namespace grotto::tui {

namespace {

std::string format_uptime(int64_t seconds) {
    if (seconds < 60) return std::to_string(seconds) + "s";
    if (seconds < 3600) {
        int m = static_cast<int>(seconds / 60);
        int s = static_cast<int>(seconds % 60);
        return std::to_string(m) + "m " + std::to_string(s) + "s";
    }
    if (seconds < 86400) {
        int h = static_cast<int>(seconds / 3600);
        int m = static_cast<int>((seconds % 3600) / 60);
        return std::to_string(h) + "h " + std::to_string(m) + "m";
    }
    int d = static_cast<int>(seconds / 86400);
    int h = static_cast<int>((seconds % 86400) / 3600);
    return std::to_string(d) + "d " + std::to_string(h) + "h";
}

} // anonymous namespace

AdminTui::AdminTui(const std::string& socket_path)
    : client_()
    , screen_(ftxui::ScreenInteractive::Fullscreen())
    , socket_path_(socket_path)
    , log_view_()
    , user_view_([this](const std::string& action, const std::string& user_id) {
          on_user_action(action, user_id);
      })
    , settings_view_([this](const std::string& key, const std::string& value) {
          on_config_change(key, value);
      })
    , bugreport_view_([this](int id, const std::string& status) {
          on_bugreport_action(id, status);
      })
{
}

int AdminTui::run() {
    // Set event callback
    client_.set_event_callback([this](const nlohmann::json& event) {
        on_event(event);
        screen_.PostEvent(ftxui::Event::Custom);
    });

    // Try to connect
    if (client_.connect(socket_path_)) {
        status_text_ = "Connected";
        nlohmann::json events_array = nlohmann::json::array(
            {"log", "users", "channels", "stats", "config", "bug_reports"});
        client_.send_command(protocol::to_json_command("subscribe",
            {{"events", events_array}}));
    }

    // Build component tree
    auto input = ftxui::Input(&command_input_, "> ");

    auto renderer = ftxui::Renderer(input, [&] {
        using namespace ftxui;

        // Tab bar
        auto tab_bar = hbox({
            text(active_tab_ == 0 ? "[F1 Loki]" : " F1 Loki ")
                | (active_tab_ == 0 ? inverted : nothing),
            text(" "),
            text(active_tab_ == 1 ? "[F2 Asetukset]" : " F2 Asetukset ")
                | (active_tab_ == 1 ? inverted : nothing),
            text(" "),
            text(active_tab_ == 2 ? "[F3 Bug Reports]" : " F3 Bug Reports ")
                | (active_tab_ == 2 ? inverted : nothing),
            filler(),
            text("v" + server_version_) | color(Color::GrayDark),
        });

        // Left pane: active view
        Element left_pane;
        switch (active_tab_) {
            case 0:  left_pane = log_view_.render(); break;
            case 1:  left_pane = settings_view_.render(); break;
            case 2:  left_pane = bugreport_view_.render(); break;
            default: left_pane = log_view_.render(); break;
        }

        // Right pane: sidebar
        auto right_pane = user_view_.render();

        // Status bar
        auto status_bar = hbox({
            text(" " + status_text_ + " ") | color(
                client_.connected() ? Color::Green : Color::Red),
            filler(),
            text("Users: " + std::to_string(connection_count_))
                | color(Color::GrayDark),
            text("  "),
            text("Uptime: " + format_uptime(server_uptime_))
                | color(Color::GrayDark),
        });

        // Main layout: content + sidebar
        auto main_content = hbox({
            left_pane | flex_grow,
            separator(),
            right_pane | size(WIDTH, EQUAL, 25),
        });

        return vbox({
            tab_bar,
            separator(),
            main_content | flex,
            separator(),
            status_bar,
            separator(),
            input->Render(),
        }) | border;
    });

    // Event handler
    auto event_handler = ftxui::CatchEvent(renderer, [&](ftxui::Event event) -> bool {
        // F1-F3 tab switching
        if (event == ftxui::Event::F1) { active_tab_ = 0; return true; }
        if (event == ftxui::Event::F2) { active_tab_ = 1; return true; }
        if (event == ftxui::Event::F3) { active_tab_ = 2; return true; }

        // Ctrl+D = quit/detach
        if (event == ftxui::Event::Special("\x04")) {
            screen_.Exit();
            return true;
        }

        // Enter on command input
        if (event == ftxui::Event::Return && !command_input_.empty()) {
            handle_command_input(command_input_);
            command_input_.clear();
            return true;
        }

        // Mouse events -> user view
        if (event.is_mouse()) {
            return user_view_.on_mouse(event);
        }

        // Scroll events -> log view (when on log tab)
        if (active_tab_ == 0) {
            if (event == ftxui::Event::ArrowUp) return log_view_.on_scroll(1);
            if (event == ftxui::Event::ArrowDown) return log_view_.on_scroll(-1);
        }

        // Key events -> active view
        if (active_tab_ == 1) return settings_view_.on_key(event);
        if (active_tab_ == 2) return bugreport_view_.on_key(event);

        // Context menu keys
        if (user_view_.on_key(event)) return true;

        return false;
    });

    screen_.Loop(event_handler);
    client_.disconnect();
    return 0;
}

void AdminTui::on_event(const nlohmann::json& event) {
    try {
        std::string type = event.value("type", "");
        if (!event.contains("data")) return;  // skip command responses etc.
        const auto& data = event["data"];

        if (type == "log") {
            auto entry = data.get<protocol::LogEntry>();
            log_view_.push(std::move(entry));
        } else if (type == "users") {
            auto users = data.get<std::vector<protocol::UserInfo>>();
            user_view_.set_users(std::move(users));
            connection_count_ = static_cast<int>(users.size());
        } else if (type == "channels") {
            auto channels = data.get<std::vector<protocol::ChannelInfo>>();
            user_view_.set_channels(std::move(channels));
        } else if (type == "stats") {
            auto stats = data.get<protocol::ServerStats>();
            server_uptime_ = stats.uptime;
            connection_count_ = stats.connections;
            server_version_ = stats.version;
        } else if (type == "config") {
            auto entries = data.get<std::vector<protocol::ConfigEntry>>();
            settings_view_.set_config(std::move(entries));
        } else if (type == "bug_reports") {
            auto reports = data.get<std::vector<protocol::BugReport>>();
            bugreport_view_.set_reports(std::move(reports));
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to parse event: {}", e.what());
    }
}

void AdminTui::on_user_action(const std::string& action, const std::string& user_id) {
    if (action == "kick") {
        protocol::KickCommand cmd{user_id, ""};
        client_.send_command(protocol::to_json_command("kick", cmd));
    } else if (action == "ban") {
        protocol::BanCommand cmd{user_id, ""};
        client_.send_command(protocol::to_json_command("ban", cmd));
    } else if (action == "whois") {
        client_.send_command(protocol::to_json_command("whois",
            nlohmann::json{{"user_id", user_id}}));
    }
}

void AdminTui::on_config_change(const std::string& key, const std::string& value) {
    protocol::SetConfigCommand cmd{key, value};
    client_.send_command(protocol::to_json_command("set_config", cmd));
}

void AdminTui::on_bugreport_action(int id, const std::string& status) {
    protocol::UpdateBugReportCommand cmd{id, status};
    client_.send_command(protocol::to_json_command("update_bug_report", cmd));
}

void AdminTui::handle_command_input(const std::string& input) {
    if (input.empty() || input[0] != '/') {
        log_view_.push(protocol::LogEntry{"warn", "Unknown command. Use /kick, /ban, /whois", ""});
        return;
    }

    std::istringstream iss(input.substr(1));
    std::string cmd;
    iss >> cmd;

    if (cmd == "kick") {
        std::string user;
        iss >> user;
        std::string reason;
        std::getline(iss, reason);
        if (!reason.empty() && reason[0] == ' ') reason = reason.substr(1);
        if (!user.empty()) {
            protocol::KickCommand kc{user, reason};
            client_.send_command(protocol::to_json_command("kick", kc));
            log_view_.push(protocol::LogEntry{"info", "Kick command sent: " + user, ""});
        }
    } else if (cmd == "ban") {
        std::string user;
        iss >> user;
        std::string reason;
        std::getline(iss, reason);
        if (!reason.empty() && reason[0] == ' ') reason = reason.substr(1);
        if (!user.empty()) {
            protocol::BanCommand bc{user, reason};
            client_.send_command(protocol::to_json_command("ban", bc));
            log_view_.push(protocol::LogEntry{"info", "Ban command sent: " + user, ""});
        }
    } else if (cmd == "whois") {
        std::string user;
        iss >> user;
        if (!user.empty()) {
            client_.send_command(protocol::to_json_command("whois",
                nlohmann::json{{"user_id", user}}));
            log_view_.push(protocol::LogEntry{"info", "Whois query sent: " + user, ""});
        }
    } else {
        log_view_.push(protocol::LogEntry{"warn", "Unknown command: /" + cmd, ""});
    }
}

} // namespace grotto::tui
