#include "config.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace {

std::filesystem::path make_temp_dir(const char* tag) {
    const auto tick = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      (std::string("grotto-") + tag + "-" + std::to_string(tick));
    std::filesystem::create_directories(path);
    return path;
}

void cleanup(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

} // namespace

TEST_CASE("voice ICE settings survive save/load", "[config]") {
    const auto dir = make_temp_dir("config-save");
    const auto path = dir / "client.toml";

    grotto::ClientConfig cfg;
    cfg.voice.ice_servers = {
        "stun:turn.example.com:3478",
        "turn:turn.example.com:3478?transport=udp",
        "turns:turn.example.com:5349?transport=tcp",
    };
    cfg.voice.turn_username = "grotto";
    cfg.voice.turn_password = "secret";
    cfg.voice.noise_suppression_enabled = false;
    cfg.voice.noise_suppression_level = "very_high";
    cfg.voice.limiter_enabled = true;
    cfg.voice.limiter_threshold = 0.73f;
    cfg.preview.terminal_graphics = "viewer-only";

    grotto::save_config(cfg, path);
    const auto loaded = grotto::load_config(path);

    REQUIRE(loaded.voice.ice_servers == cfg.voice.ice_servers);
    REQUIRE(loaded.voice.turn_username == cfg.voice.turn_username);
    REQUIRE(loaded.voice.turn_password == cfg.voice.turn_password);
    REQUIRE(loaded.voice.noise_suppression_enabled == cfg.voice.noise_suppression_enabled);
    REQUIRE(loaded.voice.limiter_enabled == cfg.voice.limiter_enabled);
    REQUIRE(loaded.voice.limiter_threshold == Catch::Approx(cfg.voice.limiter_threshold));
    REQUIRE(loaded.preview.terminal_graphics == cfg.preview.terminal_graphics);

    std::ifstream saved(path);
    REQUIRE(saved.good());
    std::ostringstream saved_text;
    saved_text << saved.rdbuf();
    REQUIRE(saved_text.str().find("noise_suppression_level") == std::string::npos);

    cleanup(dir);
}

TEST_CASE("voice ICE settings survive export/import", "[config]") {
    const auto dir = make_temp_dir("config-export");
    const auto path = dir / "settings.toml";

    grotto::ClientConfig original;
    original.voice.ice_servers = {
        "stun:turn.example.com:3478",
        "turn:turn.example.com:3478?transport=udp",
    };
    original.voice.turn_username = "relay";
    original.voice.turn_password = "relay-secret";
    original.voice.noise_suppression_enabled = true;
    original.voice.noise_suppression_level = "high";
    original.voice.limiter_enabled = true;
    original.voice.limiter_threshold = 0.81f;
    original.preview.terminal_graphics = "off";

    grotto::export_settings(original, path);

    grotto::ClientConfig imported;
    REQUIRE(grotto::import_settings(imported, path));
    REQUIRE(imported.voice.ice_servers == original.voice.ice_servers);
    REQUIRE(imported.voice.turn_username == original.voice.turn_username);
    REQUIRE(imported.voice.turn_password == original.voice.turn_password);
    REQUIRE(imported.voice.noise_suppression_enabled == original.voice.noise_suppression_enabled);
    REQUIRE(imported.voice.limiter_enabled == original.voice.limiter_enabled);
    REQUIRE(imported.voice.limiter_threshold == Catch::Approx(original.voice.limiter_threshold));
    REQUIRE(imported.preview.terminal_graphics == original.preview.terminal_graphics);

    std::ifstream exported(path);
    REQUIRE(exported.good());
    std::ostringstream exported_text;
    exported_text << exported.rdbuf();
    REQUIRE(exported_text.str().find("noise_suppression_level") == std::string::npos);

    cleanup(dir);
}

TEST_CASE("legacy noise suppression level still loads from config", "[config]") {
    const auto dir = make_temp_dir("config-legacy-level");
    const auto path = dir / "client.toml";

    {
        std::ofstream out(path);
        out << "[voice]\n";
        out << "noise_suppression_enabled = true\n";
        out << "noise_suppression_level = \"very_high\"\n";
        out << "limiter_enabled = false\n";
    }

    const auto loaded = grotto::load_config(path);
    REQUIRE(loaded.voice.noise_suppression_enabled);
    REQUIRE(loaded.voice.noise_suppression_level == "very_high");
    REQUIRE_FALSE(loaded.voice.limiter_enabled);

    cleanup(dir);
}

TEST_CASE("legacy ptt voice mode loads as toggle mode", "[config]") {
    const auto dir = make_temp_dir("config-legacy-voice-mode");
    const auto path = dir / "client.toml";

    {
        std::ofstream out(path);
        out << "[voice]\n";
        out << "mode = \"ptt\"\n";
        out << "ptt_key = \"F1\"\n";
    }

    const auto loaded = grotto::load_config(path);
    REQUIRE(loaded.voice.mode == "toggle");
    REQUIRE(loaded.voice.ptt_key == "F1");

    cleanup(dir);
}
