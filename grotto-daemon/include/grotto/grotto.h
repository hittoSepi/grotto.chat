#pragma once

#include <array>
#include <string>

namespace grotto {

class GROTTO {
public:
    struct FaceLines {
        std::array<std::string, 5> lines;
    };

    enum class mood {
        sleeping,
        waking_up,
        calm,
        focused,
        restless,
        agitated,
        grumpy,
        on_edge,
        MAD,
    };

    GROTTO();

    std::string get_mood() const;
    std::string get_face() const;
    FaceLines get_face_lines() const;

    void set_mood(mood m);
    void randomize_mood();

    std::string random_startup_message();
    std::string random_error_message();
    std::string random_progress_message();

    void print_status(const std::string& message = "") const;

    static const std::string grotto_daemon;
    static const std::string banner_large;
    static const std::string banner_small;

private:
    mood current_mood_;
};

}  // namespace grotto
