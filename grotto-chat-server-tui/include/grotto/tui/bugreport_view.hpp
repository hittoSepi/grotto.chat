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

    // FTXUI render
    ftxui::Element render();

    // Handle key events
    bool on_key(ftxui::Event& event);

private:
    ftxui::Color status_color(const std::string& status);
    std::string next_status(const std::string& current);

    std::vector<protocol::BugReport> reports_;
    std::mutex mutex_;
    int selected_ = 0;
    bool show_detail_ = false;
    BugReportActionCallback on_action_;
};

} // namespace grotto::tui
