#include "app.hpp"

#include "grotto/grotto.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <chrono>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace grotto::installer {

using namespace ftxui;

namespace {

Element BoxTitle(const std::string& text_value) {
    return text(text_value) | bold;
}

Element ParagraphLines(const std::vector<std::string>& lines) {
    Elements children;
    for (const auto& line : lines) {
        children.push_back(paragraph(line));
    }
    if (children.empty()) {
        children.push_back(text(""));
    }
    return vbox(std::move(children));
}

std::vector<std::string> FormatPreflight(const std::vector<PreflightIssue>& issues) {
    std::vector<std::string> lines;
    if (issues.empty()) {
        lines.push_back("[ok] Preflight passed with current settings.");
        return lines;
    }

    for (const auto& issue : issues) {
        lines.push_back(std::string(issue.blocking ? "[block] " : "[warn] ") + issue.message);
    }
    return lines;
}

std::string TlsLabel(TlsMode mode) {
    switch (mode) {
        case TlsMode::LetsEncrypt:
            return "Let's Encrypt";
        case TlsMode::SelfSigned:
            return "Self-signed";
        case TlsMode::ExistingCert:
            return "Existing cert";
    }
    return "Unknown";
}

std::vector<std::string> SplitLines(const std::string& text_value) {
    std::vector<std::string> lines;
    std::istringstream stream(text_value);
    for (std::string line; std::getline(stream, line);) {
        lines.push_back(line);
    }
    return lines;
}

Elements BannerLines() {
    return {
        text(" ██████╗ ██████╗  ██████╗ ████████╗████████╗ ██████╗ "),
        text("██╔════╝ ██╔══██╗██╔═══██╗╚══██╔══╝╚══██╔══╝██╔═══██╗"),
        text("██║  ███╗██████╔╝██║   ██║   ██║      ██║   ██║   ██║"),
        text("██║   ██║██╔══██╗██║   ██║   ██║      ██║   ██║   ██║"),
        text("╚██████╔╝██║  ██║╚██████╔╝   ██║      ██║   ╚██████╔╝"),
        text(" ╚═════╝ ╚═╝  ╚═╝ ╚═════╝    ╚═╝      ╚═╝    ╚═════╝ "),
    };
}

Elements BannerSmall() {
    return {
        text("GROTTO.chat"),
        text("installer"),
    };
}

}  // namespace

enum class InstallStepStatus { pending, running, ok, error };

struct InstallStep {
    std::string label;
    InstallStepStatus status = InstallStepStatus::pending;
};

struct InstallerApp::Impl {
    explicit Impl(SystemInfo detected_system,
                  Manifest detected_manifest,
                  std::string detected_manifest_source,
                  std::string detected_manifest_error)
        : system_info(std::move(detected_system)),
          manifest(std::move(detected_manifest)),
          manifest_source(std::move(detected_manifest_source)),
          manifest_error(std::move(detected_manifest_error)),
          runner(system_info, manifest) {
        InitializePacks();
    }

    void InitializePacks() {
        if (!manifest.packs.empty()) {
            for (const auto& pack : manifest.packs) {
                pack_ui_states.push_back(pack.id == "server");
                pack_ids.push_back(pack.id);
            }
        } else {
            pack_ids = {"server"};
            pack_ui_states = {true};
        }
    }

    int Run() {
        BuildComponents();
        RefreshPreflight();
        screen.Loop(root);
        return 0;
    }

    ButtonOption StyledButton() {
        ButtonOption option;
        option.transform = [](const EntryState& s) {
            auto element = text(s.label) | border;
            if (s.focused) {
                element |= inverted;
            }
            return element;
        };
        return option;
    }

    void BuildComponents() {
        tls_entries = {"Let's Encrypt", "Self-signed", "Existing cert"};
        tls_index = static_cast<int>(config.tls_mode);

        auto btn_opt = StyledButton();

        // Welcome
        welcome_buttons = Container::Horizontal({
            Button("Re-run preflight", [&] { RefreshPreflight(); }, btn_opt),
            Button("Continue ", [&] {
                RefreshPreflight();
                active_tab = 1;
            }, btn_opt),
        });

        // Pack selection
        pack_checkboxes.clear();
        pack_checkbox_states.clear();
        for (size_t i = 0; i < pack_ids.size(); ++i) {
            pack_checkbox_states.push_back(std::make_unique<PackCheckboxState>(PackCheckboxState{pack_ui_states[i] != 0}));
        }
        for (size_t i = 0; i < pack_ids.size(); ++i) {
            pack_checkboxes.push_back(Checkbox(pack_ids[i], &pack_checkbox_states[i]->checked));
        }
        pack_list = Container::Vertical(pack_checkboxes);
        
        pack_buttons = Container::Horizontal({
            Button(" Back ", [&] { active_tab = 0; }, btn_opt),
            Button("Configure ", [&] {
                UpdateSelectedPacks();
                active_tab = 2;
            }, btn_opt),
        });

        // Configuration
        domain_input = Input(&config.domain, "chat.example.com");
        listen_input = Input(&config.listen_address, "0.0.0.0");
        port_input = Input(&config.port_text, "6697");
        install_path_input = Input(&config.install_path, "/opt/grotto");
        data_path_input = Input(&config.data_path, "/var/lib/grotto");
        cert_input = Input(&config.existing_cert_path, "/etc/grotto/cert.pem");
        key_input = Input(&config.existing_key_path, "/etc/grotto/key.pem");
        tls_selector = Radiobox(&tls_entries, &tls_index);
        firewall_checkbox = Checkbox("Manage firewall with UFW", &config.manage_firewall);
        install_ufw_checkbox = Checkbox("Install UFW if missing", &config.install_ufw_if_missing);
        start_checkbox = Checkbox("Start after install", &config.start_after_install);

        configure_buttons = Container::Horizontal({
            Button(" Back ", [&] { active_tab = 1; }, btn_opt),
            Button("Review ", [&] {
                SyncTlsMode();
                RefreshPreflight();
                BuildInstallSteps();
                active_tab = 3;
            }, btn_opt),
        });

        configure_form = Container::Vertical({
            domain_input,
            listen_input,
            port_input,
            install_path_input,
            data_path_input,
            tls_selector,
            cert_input,
            key_input,
            firewall_checkbox,
            install_ufw_checkbox,
            start_checkbox,
            configure_buttons,
        });

        // Confirm
        confirm_buttons = Container::Horizontal({
            Button(" Back ", [&] { active_tab = 2; }, btn_opt),
            Button("Install", [&] {
                SyncTlsMode();
                RefreshPreflight();
                bool blocked = false;
                for (const auto& issue : preflight_issues) {
                    if (issue.blocking) {
                        blocked = true;
                        break;
                    }
                }
                if (!blocked) {
                    StartInstall();
                }
            }, btn_opt),
        });

        // Progress
        progress_buttons = Container::Horizontal({
            Button("Close", [&] {
                if (install_done) {
                    screen.ExitLoopClosure()();
                }
            }, btn_opt),
        });

        // Result
        result_buttons = Container::Horizontal({
            Button("Exit", [&] { screen.ExitLoopClosure()(); }, btn_opt),
        });

        // Renderers
        welcome_renderer = Renderer(welcome_buttons, [&] { return RenderWelcome(); });
        pack_renderer = Renderer(Container::Vertical({pack_list, pack_buttons}), [&] { return RenderPackSelect(); });
        configure_renderer = Renderer(configure_form, [&] { return RenderConfigure(); });
        confirm_renderer = Renderer(confirm_buttons, [&] { return RenderConfirm(); });
        progress_renderer = Renderer(progress_buttons, [&] { return RenderProgress(); });
        result_renderer = Renderer(result_buttons, [&] { return RenderResult(); });

        tabs = Container::Tab({
                                  welcome_renderer,
                                  pack_renderer,
                                  configure_renderer,
                                  confirm_renderer,
                                  progress_renderer,
                                  result_renderer,
                              },
                              &active_tab);

        root = Renderer(tabs, [&] {
            return window(
                       BoxTitle("GROTTO.chat Installer"),
                       tabs->Render() | flex) |
                   size(WIDTH, GREATER_THAN, 90) |
                   size(HEIGHT, GREATER_THAN, 32);
        });
    }

    void SyncTlsMode() {
        config.tls_mode = static_cast<TlsMode>(tls_index);
    }

    void RefreshPreflight() {
        SyncTlsMode();
        preflight_issues = runner.RunPreflight(config);
    }

    void UpdateSelectedPacks() {
        config.selected_pack_ids.clear();
        for (size_t i = 0; i < pack_ids.size() && i < pack_checkbox_states.size(); ++i) {
            if (pack_checkbox_states[i]->checked) {
                config.selected_pack_ids.push_back(pack_ids[i]);
            }
        }
        if (config.selected_pack_ids.empty()) {
            config.selected_pack_ids.push_back("server");
            if (!pack_checkbox_states.empty()) {
                pack_checkbox_states[0]->checked = true;
            }
        }
    }

    void BuildInstallSteps() {
        install_steps.clear();
        install_steps.push_back({"summoning grotto daemon...", InstallStepStatus::pending});
        install_steps.push_back({"lighting encryption torches...", InstallStepStatus::pending});
        install_steps.push_back({"installing dependencies", InstallStepStatus::pending});
        
        for (const auto& pack_id : config.selected_pack_ids) {
            install_steps.push_back({"downloading " + pack_id + "...", InstallStepStatus::pending});
            install_steps.push_back({"verifying " + pack_id + "...", InstallStepStatus::pending});
            install_steps.push_back({"installing " + pack_id + "...", InstallStepStatus::pending});
        }
        
        install_steps.push_back({"configuring TLS", InstallStepStatus::pending});
        install_steps.push_back({"writing server config", InstallStepStatus::pending});
        install_steps.push_back({"installing systemd service", InstallStepStatus::pending});
        install_steps.push_back({"finalizing cave layout...", InstallStepStatus::pending});
        
        if (config.start_after_install) {
            install_steps.push_back({"starting GROTTO.chat", InstallStepStatus::pending});
        }
    }

    void PushLog(const std::string& line) {
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            progress_lines.push_back(line);
        }
        screen.PostEvent(Event::Custom);
    }

    void SetStepStatus(size_t index, InstallStepStatus status) {
        if (index < install_steps.size()) {
            std::lock_guard<std::mutex> lock(log_mutex);
            install_steps[index].status = status;
            current_step = index;
        }
        screen.PostEvent(Event::Custom);
    }

    void StartInstall() {
        if (install_thread.joinable()) {
            install_thread.join();
        }

        install_done = false;
        install_summary = {};
        current_step = 0;
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            progress_lines.clear();
        }

        daemon.randomize_mood();
        active_tab = 4;
        
        install_thread = std::thread([this] {
            using namespace std::chrono_literals;

            // Boot sequence animation
            daemon.set_mood(grotto::GROTTO::mood::sleeping);
            std::this_thread::sleep_for(300ms);
            SetStepStatus(0, InstallStepStatus::running);
            
            daemon.set_mood(grotto::GROTTO::mood::waking_up);
            std::this_thread::sleep_for(300ms);
            SetStepStatus(0, InstallStepStatus::ok);
            SetStepStatus(1, InstallStepStatus::running);
            
            daemon.set_mood(grotto::GROTTO::mood::focused);
            std::this_thread::sleep_for(200ms);
            SetStepStatus(1, InstallStepStatus::ok);

            // Run actual install
            size_t step_idx = 2;
            install_summary = runner.RunInstall(config, [this, &step_idx](const std::string& step, bool is_error) {
                if (step_idx < install_steps.size()) {
                    SetStepStatus(step_idx, is_error ? InstallStepStatus::error : InstallStepStatus::ok);
                    step_idx++;
                    if (step_idx < install_steps.size()) {
                        SetStepStatus(step_idx, InstallStepStatus::running);
                    }
                }
                PushLog((is_error ? "[error] " : "[step] ") + step);
            });

            // Final status
            install_done = true;
            if (install_summary.success) {
                daemon.set_mood(grotto::GROTTO::mood::calm);
            } else {
                daemon.set_mood(grotto::GROTTO::mood::grumpy);
            }
            active_tab = 5;
            screen.PostEvent(Event::Custom);
        });
    }

    Element RenderHeader(bool compact = false) const {
        auto banner = compact ? BannerSmall() : BannerLines();
        auto face = daemon.get_face_lines();
        
        Elements face_elements;
        for (const auto& line : face.lines) {
            face_elements.push_back(text(line));
        }
        face_elements.push_back(text(""));
        face_elements.push_back(text("daemon mood: " + daemon.get_mood()) | color(Color::Green));
        
        return hbox({
            vbox(std::move(banner)) | flex,
            separator(),
            vbox(std::move(face_elements))
        });
    }

    Element RenderWelcome() const {
        auto header = RenderHeader(false);
        
        std::vector<std::string> info = {
            "Platform: " + system_info.platform_key,
            "systemd: " + std::string(system_info.has_systemd ? "yes" : "no"),
            "UFW: " + std::string(system_info.has_ufw ? "yes" : "no"),
            "",
            "Manifest: " + manifest_source,
            "Version: " + manifest.version,
        };
        
        if (!manifest_error.empty()) {
            info.push_back("");
            info.push_back("Warning: " + manifest_error);
        }

        return vbox({
                   header,
                   separator(),
                   ParagraphLines(info),
                   separator(),
                   ParagraphLines(FormatPreflight(preflight_issues)),
                   separator(),
                   welcome_buttons->Render(),
               }) |
               border;
    }

    Element RenderPackSelect() const {
        auto header = RenderHeader(true);
        
        // Show first selected pack description, or first pack if none selected
        std::string description = "Select packs to install.";
        if (!manifest.packs.empty()) {
            for (size_t i = 0; i < manifest.packs.size() && i < pack_ui_states.size(); ++i) {
                if (pack_ui_states[i]) {
                    description = manifest.packs[i].description;
                    break;
                }
            }
            if (description == "Select packs to install.") {
                description = manifest.packs[0].description;
            }
        }

        auto left = vbox({
            text("SELECT PACKS TO INSTALL") | bold,
            separator(),
            pack_list->Render(),
        }) | border;

        auto right = vbox({
            text("Description") | bold,
            separator(),
            paragraph(description),
        }) | border | flex;

        return vbox({
                   header,
                   separator(),
                   hbox({left, right}),
                   separator(),
                   pack_buttons->Render(),
               }) |
               border;
    }

    Element RenderConfigure() const {
        auto header = RenderHeader(true);
        
        Elements rows = {
            text("Configuration") | bold,
            separator(),
            hbox(text("Domain:            "), domain_input->Render()),
            hbox(text("Listen address:    "), listen_input->Render()),
            hbox(text("Port:              "), port_input->Render()),
            hbox(text("Install path:      "), install_path_input->Render()),
            hbox(text("Data path:         "), data_path_input->Render()),
            text("TLS mode:"),
            tls_selector->Render(),
        };

        if (static_cast<TlsMode>(tls_index) == TlsMode::ExistingCert) {
            rows.push_back(hbox(text("Existing cert:     "), cert_input->Render()));
            rows.push_back(hbox(text("Existing key:      "), key_input->Render()));
        }

        rows.push_back(firewall_checkbox->Render());
        if (!system_info.has_ufw) {
            rows.push_back(install_ufw_checkbox->Render());
        }
        rows.push_back(start_checkbox->Render());
        rows.push_back(separator());
        rows.push_back(configure_buttons->Render());

        return vbox({
                   header,
                   separator(),
                   vbox(std::move(rows)),
               }) |
               border;
    }

    Element RenderConfirm() const {
        auto header = RenderHeader(true);
        
        std::vector<std::string> summary = {
            "Review installation:",
            "",
            "Packs: " + [&] {
                std::string s;
                for (const auto& id : config.selected_pack_ids) {
                    if (!s.empty()) s += ", ";
                    s += id;
                }
                return s;
            }(),
            "Domain: " + config.domain,
            "Listen: " + config.listen_address + ":" + config.port_text,
            "Install path: " + config.install_path,
            "TLS: " + TlsLabel(config.tls_mode),
            "",
            "Preflight:",
        };

        return vbox({
                   header,
                   separator(),
                   ParagraphLines(summary),
                   ParagraphLines(FormatPreflight(preflight_issues)),
                   separator(),
                   confirm_buttons->Render(),
               }) |
               border;
    }

    Element RenderProgress() const {
        auto header = RenderHeader(true);
        
        Elements step_elements;
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            for (const auto& step : install_steps) {
                std::string marker;
                Element marker_elem;
                switch (step.status) {
                    case InstallStepStatus::pending:
                        marker = "[  ]";
                        marker_elem = text(marker);
                        break;
                    case InstallStepStatus::running:
                        marker = "[..]";
                        marker_elem = text(marker) | color(Color::Yellow);
                        break;
                    case InstallStepStatus::ok:
                        marker = "[ok]";
                        marker_elem = text(marker) | color(Color::Green);
                        break;
                    case InstallStepStatus::error:
                        marker = "[!!]";
                        marker_elem = text(marker) | color(Color::Red);
                        break;
                }
                step_elements.push_back(hbox({
                    marker_elem,
                    text(" "),
                    text(step.label)
                }));
            }
        }

        // Progress bar
        float progress = 0.0f;
        if (!install_steps.empty()) {
            int completed = 0;
            {
                std::lock_guard<std::mutex> lock(log_mutex);
                for (const auto& step : install_steps) {
                    if (step.status == InstallStepStatus::ok || step.status == InstallStepStatus::error) {
                        completed++;
                    }
                }
            }
            progress = static_cast<float>(completed) / static_cast<float>(install_steps.size());
        }

        return vbox({
                   header,
                   separator(),
                   vbox(std::move(step_elements)),
                   separator(),
                   gauge(progress),
                   separator(),
                   text(install_done ? "Installation finished." : "Installing..."),
                   progress_buttons->Render(),
               }) |
               border;
    }

    Element RenderResult() const {
        auto header = RenderHeader(true);
        auto face = daemon.get_face_lines();
        
        Elements lines = {
            install_summary.headline.empty() ? text("Installation finished") : text(install_summary.headline),
        };
        
        if (install_summary.success) {
            lines.push_back(text(daemon.random_startup_message()) | color(Color::Green));
        } else {
            lines.push_back(text(daemon.random_error_message()) | color(Color::Red));
        }
        
        for (const auto& detail : install_summary.details) {
            lines.push_back(text(detail));
        }

        return vbox({
                   header,
                   separator(),
                   vbox(std::move(lines)),
                   separator(),
                   result_buttons->Render(),
               }) |
               border;
    }

    ScreenInteractive screen = ScreenInteractive::Fullscreen();
    SystemInfo system_info;
    Manifest manifest;
    std::string manifest_source;
    std::string manifest_error;
    InstallConfig config;
    InstallerRunner runner;
    mutable grotto::GROTTO daemon;

    std::vector<PreflightIssue> preflight_issues;
    std::vector<std::string> progress_lines;
    InstallSummary install_summary;

    std::vector<std::string> pack_ids;
    std::vector<char> pack_ui_states;
    struct PackCheckboxState { bool checked; };
    std::vector<std::unique_ptr<PackCheckboxState>> pack_checkbox_states;
    std::vector<InstallStep> install_steps;
    size_t current_step = 0;

    int active_tab = 0;
    int tls_index = 0;
    std::vector<std::string> tls_entries;

    bool install_done = false;
    std::thread install_thread;
    mutable std::mutex log_mutex;

    Component root;
    Component tabs;
    Component welcome_buttons;
    Component pack_list;
    Component pack_buttons;
    std::vector<Component> pack_checkboxes;
    Component configure_form;
    Component configure_buttons;
    Component confirm_buttons;
    Component progress_buttons;
    Component result_buttons;
    Component domain_input;
    Component listen_input;
    Component port_input;
    Component install_path_input;
    Component data_path_input;
    Component cert_input;
    Component key_input;
    Component tls_selector;
    Component firewall_checkbox;
    Component install_ufw_checkbox;
    Component start_checkbox;
    Component welcome_renderer;
    Component pack_renderer;
    Component configure_renderer;
    Component confirm_renderer;
    Component progress_renderer;
    Component result_renderer;
};

InstallerApp::InstallerApp(SystemInfo system_info,
                           Manifest manifest,
                           std::string manifest_source,
                           std::string manifest_error)
    : impl_(std::make_unique<Impl>(std::move(system_info),
                                   std::move(manifest),
                                   std::move(manifest_source),
                                   std::move(manifest_error))) {}

InstallerApp::~InstallerApp() {
    if (impl_ != nullptr && impl_->install_thread.joinable()) {
        impl_->install_thread.join();
    }
}

int InstallerApp::Run() {
    return impl_->Run();
}

}  // namespace grotto::installer
