#include <grotto/tui/user_view.hpp>

#include <algorithm>
#include <string>

using namespace ftxui;

namespace grotto::tui {

namespace {
constexpr int kMenuItemCount = 3;
const std::string kMenuItems[] = {"Kick", "Ban", "Whois"};
const std::string kMenuActions[] = {"kick", "ban", "whois"};

// Lines before user list: title + separator = 2 lines, plus 1 for border top
constexpr int kUserListOffset = 2;
} // namespace

UserView::UserView(ActionCallback on_action)
    : on_action_(std::move(on_action)) {}

void UserView::set_users(std::vector<protocol::UserInfo> users) {
    std::lock_guard lock(mutex_);
    users_ = std::move(users);
}

void UserView::set_channels(std::vector<protocol::ChannelInfo> channels) {
    std::lock_guard lock(mutex_);
    channels_ = std::move(channels);
}

Element UserView::render() {
    std::lock_guard lock(mutex_);

    Elements user_elements;
    user_elements.push_back(text("Kayttajat (" + std::to_string(users_.size()) + ")") | bold);
    user_elements.push_back(separator());

    for (int i = 0; i < static_cast<int>(users_.size()); ++i) {
        const auto& u = users_[i];
        std::string display = u.nickname.empty() ? u.id : u.nickname;

        auto line = hbox({
            text("* ") | color(Color::Green),
            text(display),
        });

        if (i == selected_user_) {
            line = line | inverted;
        }

        user_elements.push_back(line);
    }

    user_elements.push_back(separator());
    user_elements.push_back(text("Kanavat (" + std::to_string(channels_.size()) + ")") | bold);
    user_elements.push_back(separator());

    for (const auto& ch : channels_) {
        user_elements.push_back(hbox({
            text("# ") | color(Color::Cyan),
            text(ch.name),
            text(" (" + std::to_string(ch.members) + ")") | dim,
        }));
    }

    auto sidebar = vbox(std::move(user_elements)) | border | size(WIDTH, EQUAL, 28);

    if (show_context_menu_) {
        auto menu = render_context_menu();
        // Overlay the context menu using dbox
        sidebar = dbox({
            sidebar,
            menu | clear_under | ftxui::center,
        });
    }

    return sidebar;
}

bool UserView::on_mouse(ftxui::Event& event) {
    if (!event.is_mouse()) return false;

    auto& mouse = event.mouse();

    // Right-click on a user line -> open context menu
    if (mouse.button == Mouse::Right && mouse.motion == Mouse::Released) {
        std::lock_guard lock(mutex_);
        // Calculate which user was clicked
        // Border top = 1, title = 1, separator = 1, then user lines start
        int user_index = mouse.y - 3; // adjust for border + header
        if (user_index >= 0 && user_index < static_cast<int>(users_.size())) {
            selected_user_ = user_index;
            context_user_id_ = users_[user_index].id;
            show_context_menu_ = true;
            context_menu_selected_ = 0;
            context_menu_y_ = mouse.y;
            return true;
        }
    }

    // Left-click while context menu is shown
    if (mouse.button == Mouse::Left && mouse.motion == Mouse::Released) {
        std::lock_guard lock(mutex_);
        if (show_context_menu_) {
            show_context_menu_ = false;
            selected_user_ = -1;
            return true;
        }
    }

    return false;
}

bool UserView::on_key(ftxui::Event& event) {
    std::lock_guard lock(mutex_);

    if (!show_context_menu_) return false;

    if (event == Event::ArrowUp) {
        context_menu_selected_ = std::max(0, context_menu_selected_ - 1);
        return true;
    }
    if (event == Event::ArrowDown) {
        context_menu_selected_ = std::min(kMenuItemCount - 1, context_menu_selected_ + 1);
        return true;
    }
    if (event == Event::Return) {
        std::string action = kMenuActions[context_menu_selected_];
        std::string user_id = context_user_id_;
        show_context_menu_ = false;
        selected_user_ = -1;
        // Call outside of lock would be better, but callback should be lightweight
        if (on_action_) {
            on_action_(action, user_id);
        }
        return true;
    }
    if (event == Event::Escape) {
        show_context_menu_ = false;
        selected_user_ = -1;
        return true;
    }

    return false;
}

Element UserView::render_context_menu() {
    Elements items;
    for (int i = 0; i < kMenuItemCount; ++i) {
        auto item = text(" " + kMenuItems[i] + " ");
        if (i == context_menu_selected_) {
            item = item | inverted;
        }
        items.push_back(item);
    }
    return vbox(std::move(items)) | border | bgcolor(Color::GrayDark);
}

} // namespace grotto::tui
