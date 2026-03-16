#pragma once

#include <grotto/tui/admin_protocol.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace grotto::tui {

// action: "kick", "ban", "whois"
using ActionCallback = std::function<void(const std::string& action, const std::string& user_id)>;

class UserView {
public:
    explicit UserView(ActionCallback on_action);

    // Update user list (thread-safe)
    void set_users(std::vector<protocol::UserInfo> users);

    // Update channel list (thread-safe)
    void set_channels(std::vector<protocol::ChannelInfo> channels);

    // FTXUI render sidebar element
    ftxui::Element render();

    // Handle mouse event. Returns true if handled.
    bool on_mouse(ftxui::Event& event);

    // Handle key event for context menu navigation
    bool on_key(ftxui::Event& event);

private:
    ftxui::Element render_context_menu();

    std::vector<protocol::UserInfo> users_;
    std::vector<protocol::ChannelInfo> channels_;
    std::mutex mutex_;
    int selected_user_ = -1;
    bool show_context_menu_ = false;
    int context_menu_selected_ = 0;
    int context_menu_y_ = 0;   // Y position for rendering
    std::string context_user_id_;  // Which user the menu is for
    ActionCallback on_action_;
};

} // namespace grotto::tui
