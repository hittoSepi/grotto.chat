#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace grotto {

struct ParsedCommand {
    std::string              name;   // e.g. "join", "msg", "call"
    std::vector<std::string> args;
};

// Parse a line that starts with '/'. Returns nullopt for plain messages.
// Leading '/' is expected to be present.
std::optional<ParsedCommand> parse_command(std::string_view line);

// Detect whether pasted text looks like a local file path suitable for /upload.
// Returns the resolved path only when it points to an existing regular file.
std::optional<std::filesystem::path> detect_local_file_from_paste(std::string_view text);

// Build a shell-safe /upload command line for a local file path.
std::string make_upload_command_for_path(const std::filesystem::path& path);

// All known command names (for tab completion)
const std::vector<std::string>& known_commands();

} // namespace grotto
