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
    explicit AdminTui(const std::string& socket_path = "");
    int run();

private:
    void on_event(const nlohmann::json& event);
    void on_user_action(const std::string& action, const std::string& user_id);
    void on_config_change(const std::string& key, const std::string& value);
    void on_bugreport_action(int id, const std::string& status);
    void handle_command_input(const std::string& input);

    AdminSocketClient client_;
    ftxui::ScreenInteractive screen_;
    std::string socket_path_;

    LogView log_view_;
    UserView user_view_;
    SettingsView settings_view_;
    BugReportView bugreport_view_;

    int active_tab_ = 0;        // 0=log, 1=settings, 2=bug reports
    std::string command_input_;
    std::string status_text_ = "Disconnected";
    int connection_count_ = 0;
    std::string server_version_;
    int64_t server_uptime_ = 0;
};

} // namespace grotto::tui
