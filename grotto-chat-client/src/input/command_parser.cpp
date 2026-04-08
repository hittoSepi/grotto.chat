#include "input/command_parser.hpp"

#include <cctype>
#include <unordered_map>

namespace grotto {

static const std::vector<std::string> kKnownCommands = {
    "/join", "/part", "/msg", "/me",
    "/call", "/hangup", "/accept",
    "/voice", "/mute", "/deafen", "/vmode", "/ptt",
    "/trust", "/safety",
    "/set", "/clear", "/search",
    "/quit", "/disconnect", "/status", "/help", "/reload_help", "/settings", "/version",
    "/diag",
    "/names", "/whois",
    "/upload", "/download", "/transfers", "/files", "/downloads", "/quota",
    // Aliases
    "/j", "/p", "/w", "/q", "/h", "/rh", "/st", "/ns", "/ver", "/up", "/dl", "/xfers", "/ls", "/dir",
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
    {"/xfers", "/transfers"},
    {"/ls", "/files"},
    {"/dir", "/downloads"},
    {"/ptt", "/vmode"},
};

const std::vector<std::string>& known_commands() {
    return kKnownCommands;
}

namespace {

std::string trim_ascii(std::string_view text) {
    size_t start = 0;
    while (start < text.size() &&
           std::isspace(static_cast<unsigned char>(text[start]))) {
        ++start;
    }

    size_t end = text.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }

    return std::string(text.substr(start, end - start));
}

std::string unquote(std::string_view text) {
    if (text.size() >= 2) {
        const char first = text.front();
        const char last = text.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return std::string(text.substr(1, text.size() - 2));
        }
    }
    return std::string(text);
}

std::string unescape_pasted_path(std::string_view text) {
    std::string result;
    result.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '\\' && i + 1 < text.size()) {
            const char next = text[i + 1];
            if (std::isspace(static_cast<unsigned char>(next)) ||
                next == '"' || next == '\'' || next == '(' ||
                next == ')' || next == '[' || next == ']') {
                result.push_back(next);
                ++i;
                continue;
            }
        }
        result.push_back(ch);
    }

    return result;
}

std::vector<std::string> tokenize_command(std::string_view line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;
    char quote_char = '\0';

    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];

        if (in_quotes) {
            if (ch == quote_char) {
                in_quotes = false;
                quote_char = '\0';
                continue;
            }

            if (ch == '\\' && i + 1 < line.size()) {
                const char next = line[i + 1];
                if (next == quote_char || next == '\\') {
                    current.push_back(next);
                    ++i;
                    continue;
                }
            }

            current.push_back(ch);
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        if (ch == '"' || ch == '\'') {
            in_quotes = true;
            quote_char = ch;
            continue;
        }

        if (ch == '\\' && i + 1 < line.size()) {
            const char next = line[i + 1];
            if (std::isspace(static_cast<unsigned char>(next)) ||
                next == '"' || next == '\'' || next == '\\') {
                current.push_back(next);
                ++i;
                continue;
            }
        }

        current.push_back(ch);
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

std::string quote_for_upload_command(std::string_view text) {
    std::string result;
    result.reserve(text.size() + 2);
    result.push_back('"');
    for (const char ch : text) {
        if (ch == '"') {
            result += "\\\"";
        } else {
            result.push_back(ch);
        }
    }
    result.push_back('"');
    return result;
}

} // namespace

std::optional<ParsedCommand> parse_command(std::string_view line) {
    if (line.empty() || line[0] != '/') return std::nullopt;

    // Tokenize
    const std::vector<std::string> tokens = tokenize_command(line.substr(1));

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

std::optional<std::filesystem::path> detect_local_file_from_paste(std::string_view text) {
    std::string trimmed = trim_ascii(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    if (trimmed.find('\n') != std::string::npos || trimmed.find('\r') != std::string::npos) {
        return std::nullopt;
    }

    trimmed = unescape_pasted_path(unquote(trimmed));
    if (trimmed.empty()) {
        return std::nullopt;
    }

    std::filesystem::path path(trimmed);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return std::nullopt;
    }
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        return std::nullopt;
    }
    return path;
}

std::string make_upload_command_for_path(const std::filesystem::path& path) {
    return "/upload " + quote_for_upload_command(path.string());
}

} // namespace grotto
