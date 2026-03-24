#pragma once

#include "model.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace grotto::installer {

std::optional<Manifest> LoadManifestFromFile(const std::filesystem::path& path,
                                             const std::string& platform_key,
                                             std::string* error_message);

}  // namespace grotto::installer
