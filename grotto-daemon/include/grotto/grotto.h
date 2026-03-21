#pragma once

#include <string>

namespace grotto {

class GROTTO {
public:
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

    void set_mood(mood m);
    void randomize_mood();

    std::string random_startup_message();
    std::string random_error_message();

    void print_status(const std::string& message = "") const;

    static const std::string grotto_daemon;

private:
    mood current_mood_;
};

}  // namespace grotto
