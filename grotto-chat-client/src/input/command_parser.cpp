#include "input/command_parser.hpp"
#include <unordered_map>
#include <sstream>

namespace grotto {

static const std::vector<std::string> kKnownCommands = {
    "/join", "/part", "/msg", "/me",
    "/call", "/hangup", "/accept",
    "/voice", "/mute", "/deafen", "/ptt",
    "/trust", "/safety",
    "/set", "/clear", "/search",
    "/quit", "/disconnect", "/status", "/help", "/reload_help", "/settings", "/version",
    "/names", "/whois",
    "/upload", "/download",
    // Aliases
    "/j", "/p", "/w", "/q", "/h", "/rh", "/st", "/ns", "/ver", "/up", "/dl",
};

static const std::unordered_map<std::string, std::string> kCommandAliases = {
    {"/j", "/join"},
    {"/p", "/part"},
    {"/w", "/msg"},
    {"/q", "/quit"},
    {"/h", "/help"},
    {"/rh", "/reload_help"},
    {"/st", "/status"},
    {"/ns", "/names"},
    {"/ver", "/version"},
    {"/up", "/upload"},
    {"/dl", "/download"},
};

const std::vector<std::string>& known_commands() {
    return kKnownCommands;
}

std::optional<ParsedCommand> parse_command(std::string_view line) {
    if (line.empty() || line[0] != '/') return std::nullopt;

    // Tokenize
    std::string str(line.substr(1));  // strip leading '/'
    std::istringstream ss(str);
    std::string token;
    std::vector<std::string> tokens;
    while (ss >> token) tokens.push_back(token);

    if (tokens.empty()) return std::nullopt;

    ParsedCommand cmd;
    cmd.name = "/" + tokens[0];
    // Lowercase the command name
    for (char& c : cmd.name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (auto it = kCommandAliases.find(cmd.name); it != kCommandAliases.end()) {
        cmd.name = it->second;
    }
    cmd.args = std::vector<std::string>(tokens.begin() + 1, tokens.end());
    return cmd;
}

} // namespace grotto
