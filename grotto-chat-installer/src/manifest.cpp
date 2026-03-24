#include "manifest.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace grotto::installer {

namespace {

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open manifest file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

ArtifactInfo ParseArtifact(const nlohmann::json& object, const char* key) {
    if (!object.contains(key)) {
        throw std::runtime_error(std::string("Manifest missing artifact: ") + key);
    }

    const auto& artifact = object.at(key);
    return ArtifactInfo{
        artifact.at("url").get<std::string>(),
        artifact.value("sha256", ""),
    };
}

std::vector<PackInfo> ParsePacks(const nlohmann::json& parsed) {
    std::vector<PackInfo> packs;
    if (!parsed.contains("packs")) {
        packs.push_back(PackInfo{"server", "GROTTO.chat Server", "Encrypted relay server.", false});
        return packs;
    }

    for (const auto& pack : parsed.at("packs")) {
        packs.push_back(PackInfo{
            pack.at("id").get<std::string>(),
            pack.at("name").get<std::string>(),
            pack.value("description", ""),
            pack.value("required", false),
        });
    }
    return packs;
}

}  // namespace

std::optional<Manifest> LoadManifestFromFile(const std::filesystem::path& path,
                                             const std::string& platform_key,
                                             std::string* error_message) {
    try {
        const auto parsed = nlohmann::json::parse(ReadFile(path));
        const auto& platforms = parsed.at("platforms");
        const auto& platform = platforms.at(platform_key);

        auto packs = ParsePacks(parsed);
        std::map<std::string, ArtifactInfo> artifacts;
        for (const auto& pack : packs) {
            if (platform.contains(pack.id)) {
                const auto& artifact = platform.at(pack.id);
                artifacts.emplace(pack.id,
                                  ArtifactInfo{
                                      artifact.at("url").get<std::string>(),
                                      artifact.value("sha256", ""),
                                  });
            }
        }

        if (!artifacts.contains("server") && platform.contains("server")) {
            const auto& server = platform.at("server");
            artifacts.emplace("server", ArtifactInfo{server.at("url").get<std::string>(), server.value("sha256", "")});
        }

        return Manifest{
            parsed.value("version", "unknown"),
            parsed.value("docs_url", "https://chat.rausku.com/docs"),
            ParseArtifact(platform, "installer"),
            std::move(packs),
            std::move(artifacts),
        };
    } catch (const std::exception& ex) {
        if (error_message != nullptr) {
            *error_message = ex.what();
        }
        return std::nullopt;
    }
}

}  // namespace grotto::installer
