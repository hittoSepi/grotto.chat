#include "grotto/grotto.h"

#include <array>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    using namespace std::chrono_literals;

    grotto::GROTTO daemon;

    const std::array<grotto::GROTTO::mood, 9> moods = {
        grotto::GROTTO::mood::sleeping,
        grotto::GROTTO::mood::waking_up,
        grotto::GROTTO::mood::calm,
        grotto::GROTTO::mood::focused,
        grotto::GROTTO::mood::restless,
        grotto::GROTTO::mood::agitated,
        grotto::GROTTO::mood::grumpy,
        grotto::GROTTO::mood::on_edge,
        grotto::GROTTO::mood::MAD,
    };

    std::cout << "grotto daemon mood demo\n\n";
    for (const auto mood : moods) {
        daemon.set_mood(mood);
        daemon.print_status(daemon.random_startup_message());
        std::cout << "\n";
        std::this_thread::sleep_for(120ms);
    }

    daemon.randomize_mood();
    std::cout << "random mood: " << daemon.get_mood() << "\n";
    std::cout << "startup message: " << daemon.random_startup_message() << "\n";
    std::cout << "error message: " << daemon.random_error_message() << "\n";

    return 0;
}
