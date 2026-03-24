#include <grotto/tui/bugreport_view.hpp>

#include <algorithm>
#include <string>

using namespace ftxui;

namespace grotto::tui {

BugReportView::BugReportView(BugReportActionCallback on_action)
    : on_action_(std::move(on_action)) {}

void BugReportView::set_reports(std::vector<protocol::BugReport> reports) {
    std::lock_guard<std::mutex> lock(mutex_);
    reports_ = std::move(reports);
    if (selected_ >= static_cast<int>(reports_.size())) {
        selected_ = reports_.empty() ? 0 : static_cast<int>(reports_.size()) - 1;
    }
}

ftxui::Element BugReportView::render() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (reports_.empty()) {
        return vbox({
            text("Bug Reports (0)") | bold,
            separator(),
            text("Ei raportteja.") | color(Color::GrayDark),
        });
    }

    if (show_detail_ && selected_ >= 0 && selected_ < static_cast<int>(reports_.size())) {
        const auto& r = reports_[static_cast<size_t>(selected_)];
        return vbox({
            text("Bug Report #" + std::to_string(r.id)) | bold,
            separator(),
            hbox({ text("L\xc3\xa4hett\xc3\xa4j\xc3\xa4: "), text(r.user) }),
            hbox({ text("Tila: "), text(r.status) | color(status_color(r.status)) }),
            hbox({ text("Luotu: "), text(r.created) }),
            separator(),
            text("Kuvaus:") | bold,
            paragraph(r.description),
            separator(),
            text("[Enter] Takaisin  [s] Vaihda tila") | color(Color::GrayDark),
        });
    }

    // List view
    Elements rows;
    rows.push_back(text("Bug Reports (" + std::to_string(reports_.size()) + ")") | bold);
    rows.push_back(separator());

    for (int i = 0; i < static_cast<int>(reports_.size()); ++i) {
        const auto& r = reports_[static_cast<size_t>(i)];

        // Truncate description to 50 chars
        std::string desc = r.description;
        if (desc.size() > 50) {
            desc = desc.substr(0, 47) + "...";
        }

        // Extract date portion (first 10 chars) from created timestamp
        std::string date_str = r.created.size() >= 10 ? r.created.substr(0, 10) : r.created;

        auto row = hbox({
            text("[" + r.status + "]") | color(status_color(r.status)) | size(WIDTH, EQUAL, 12),
            text(r.user) | size(WIDTH, EQUAL, 15),
            text(desc),
            text("  " + date_str) | color(Color::GrayDark),
        });

        if (i == selected_) {
            row = row | inverted;
        }

        rows.push_back(row);
    }

    rows.push_back(separator());
    rows.push_back(text("[Enter] Avaa  [s] Vaihda tila  [\xe2\x86\x91\xe2\x86\x93] Navigoi") | color(Color::GrayDark));

    return vbox(std::move(rows));
}

bool BugReportView::on_key(ftxui::Event& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (reports_.empty()) {
        return false;
    }

    if (event == Event::ArrowUp) {
        if (selected_ > 0) {
            --selected_;
        }
        return true;
    }

    if (event == Event::ArrowDown) {
        if (selected_ < static_cast<int>(reports_.size()) - 1) {
            ++selected_;
        }
        return true;
    }

    if (event == Event::Return) {
        show_detail_ = !show_detail_;
        return true;
    }

    if (event == Event::Character('s') || event == Event::Character('S')) {
        if (selected_ >= 0 && selected_ < static_cast<int>(reports_.size())) {
            auto& r = reports_[static_cast<size_t>(selected_)];
            std::string new_st = next_status(r.status);
            r.status = new_st;
            if (on_action_) {
                on_action_(r.id, new_st);
            }
        }
        return true;
    }

    return false;
}

ftxui::Color BugReportView::status_color(const std::string& status) {
    if (status == "new")      return Color::Yellow;
    if (status == "read")     return Color::White;
    if (status == "resolved") return Color::Green;
    return Color::White;
}

std::string BugReportView::next_status(const std::string& current) {
    if (current == "new")      return "read";
    if (current == "read")     return "resolved";
    if (current == "resolved") return "new";
    return "new";
}

} // namespace grotto::tui
