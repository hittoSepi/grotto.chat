#include <grotto/tui/admin_tui.hpp>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string socket_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "grotto-tui - Admin TUI for Grotto Server\n\n"
                      << "Usage: grotto-tui [options]\n\n"
                      << "Options:\n"
                      << "  --socket <path>  Admin socket path (default: auto-detect)\n"
                      << "  --help, -h       Show this help\n";
            return 0;
        }
        if (arg == "--socket" && i + 1 < argc) {
            socket_path = argv[++i];
        }
    }

    grotto::tui::AdminTui tui(socket_path);
    return tui.run();
}
