#include "grotto/grotto.h"

#include <array>
#include <iostream>
#include <random>
#include <vector>

namespace grotto {

namespace {

std::string BuildFace(const std::string& eyes, const std::string& mouth) {
    return std::string("      /\\___/\\\n") +
           "     / " + eyes + " \\\n" +
           "    ( " + mouth + " )\n" +
           "     \\  ===  /\n" +
           "      \\_____/";
}

GROTTO::FaceLines BuildFaceLines(const std::string& eyes, const std::string& mouth) {
    return GROTTO::FaceLines{{
        "      /\\___/\\",
        "     / " + eyes + " \\",
        "    ( " + mouth + " )",
        "     \\  ===  /",
        "      \\_____/ ",
    }};
}

std::string MoodName(GROTTO::mood state) {
    switch (state) {
        case GROTTO::mood::sleeping:
            return "sleeping";
        case GROTTO::mood::waking_up:
            return "waking_up";
        case GROTTO::mood::calm:
            return "calm";
        case GROTTO::mood::focused:
            return "focused";
        case GROTTO::mood::restless:
            return "restless";
        case GROTTO::mood::agitated:
            return "agitated";
        case GROTTO::mood::grumpy:
            return "grumpy";
        case GROTTO::mood::on_edge:
            return "on_edge";
        case GROTTO::mood::MAD:
            return "MAD";
    }
    return "unknown";
}

std::string MoodFace(GROTTO::mood state) {
    switch (state) {
        case GROTTO::mood::sleeping:
            return BuildFace("- -", "-_-");
        case GROTTO::mood::waking_up:
            return BuildFace("o -", "._.");
        case GROTTO::mood::calm:
            return BuildFace("o o", "^_^");
        case GROTTO::mood::focused:
            return BuildFace("o o", "-_-");
        case GROTTO::mood::restless:
            return BuildFace("O o", "o_O");
        case GROTTO::mood::agitated:
            return BuildFace("> <", ">_<");
        case GROTTO::mood::grumpy:
            return BuildFace("> <", "-_-");
        case GROTTO::mood::on_edge:
            return BuildFace("O O", "o_o");
        case GROTTO::mood::MAD:
            return BuildFace("X X", ">_<");
    }
    return BuildFace("o o", "._.");
}

GROTTO::FaceLines MoodFaceLines(GROTTO::mood state) {
    switch (state) {
        case GROTTO::mood::sleeping:
            return BuildFaceLines("- -", "-_-");
        case GROTTO::mood::waking_up:
            return BuildFaceLines("o -", "._.");
        case GROTTO::mood::calm:
            return BuildFaceLines("o o", "^_^");
        case GROTTO::mood::focused:
            return BuildFaceLines("o o", "-_-");
        case GROTTO::mood::restless:
            return BuildFaceLines("O o", "o_O");
        case GROTTO::mood::agitated:
            return BuildFaceLines("> <", ">_<");
        case GROTTO::mood::grumpy:
            return BuildFaceLines("> <", "-_-");
        case GROTTO::mood::on_edge:
            return BuildFaceLines("O O", "o_o");
        case GROTTO::mood::MAD:
            return BuildFaceLines("X X", ">_<");
    }
    return BuildFaceLines("o o", "._.");
}

std::mt19937& Rng() {
    static std::mt19937 rng(std::random_device{}());
    return rng;
}

std::string PickRandom(const std::vector<std::string>& messages) {
    if (messages.empty()) {
        return {};
    }
    std::uniform_int_distribution<std::size_t> dist(0, messages.size() - 1);
    return messages[dist(Rng())];
}

}  // namespace

const std::string GROTTO::grotto_daemon = BuildFace("o o", "._.");
const std::string GROTTO::banner_large =
    " ██████╗ ██████╗  ██████╗ ████████╗████████╗ ██████╗\n"
    "██╔════╝ ██╔══██╗██╔═══██╗╚══██╔══╝╚══██╔══╝██╔═══██╗\n"
    "██║  ███╗██████╔╝██║   ██║   ██║      ██║   ██║   ██║\n"
    "██║   ██║██╔══██╗██║   ██║   ██║      ██║   ██║   ██║\n"
    "╚██████╔╝██║  ██║╚██████╔╝   ██║      ██║   ╚██████╔╝\n"
    " ╚═════╝ ╚═╝  ╚═╝ ╚═════╝    ╚═╝      ╚═╝    ╚═════╝";
const std::string GROTTO::banner_small =
    "GROTTO.chat installer\n"
    "secure tunnel setup";

GROTTO::GROTTO()
    : current_mood_(mood::calm) {}

std::string GROTTO::get_mood() const {
    return MoodName(current_mood_);
}

std::string GROTTO::get_face() const {
    return MoodFace(current_mood_);
}

GROTTO::FaceLines GROTTO::get_face_lines() const {
    return MoodFaceLines(current_mood_);
}

void GROTTO::set_mood(mood m) {
    current_mood_ = m;
}

void GROTTO::randomize_mood() {
    static constexpr std::array<mood, 9> states = {
        mood::sleeping,
        mood::waking_up,
        mood::calm,
        mood::focused,
        mood::restless,
        mood::agitated,
        mood::grumpy,
        mood::on_edge,
        mood::MAD,
    };
    std::uniform_int_distribution<std::size_t> dist(0, states.size() - 1);
    current_mood_ = states[dist(Rng())];
}

std::string GROTTO::random_startup_message() {
    return PickRandom({
        "secure cave ready",
        "encryption torches are lit",
        "tunnels aligned and stable",
        "keys bound, channels locked",
        "voice caverns are open",
        "file transfer paths secured",
        "link preview cache warming",
        "daemon is watching the tunnels",
        "cave integrity verified",
        "all torches burning steady",
        "encrypted channels are stable",
        "the cave is yours",
    });
}

std::string GROTTO::random_error_message() {
    return PickRandom({
        "someone kicked the cave supports",
        "daemon dislikes this tunnel state",
        "torches flicker, integrity is low",
        "encrypted path failed to hold",
        "rocks moved, channel collapsed",
        "the cave is not stable right now",
        "encryption keys rattling loose",
        "tunnel ventilation compromised",
        "stalactite fell on the config",
        "daemon refuses to light this torch",
    });
}

std::string GROTTO::random_progress_message() {
    return PickRandom({
        "summoning grotto daemon...",
        "lighting encryption torches...",
        "checking tunnel integrity...",
        "binding encryption keys...",
        "opening secure channels...",
        "preparing voice caverns...",
        "securing file transfer paths...",
        "finalizing cave layout...",
    });
}

void GROTTO::print_status(const std::string& message) const {
    std::cout << get_face() << "\n";
    std::cout << "daemon mood: " << get_mood() << "\n";
    if (!message.empty()) {
        std::cout << message << "\n";
    }
}

}  // namespace grotto
