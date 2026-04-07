#include <catch2/catch_test_macros.hpp>

#include "input/command_parser.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

std::filesystem::path make_temp_dir(const char* tag) {
    const auto tick = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      (std::string("grotto-command-parser-") + tag + "-" + std::to_string(tick));
    std::filesystem::create_directories(path);
    return path;
}

void cleanup(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

} // namespace

TEST_CASE("parse_command keeps quoted upload paths together", "[command-parser]") {
    const auto parsed = grotto::parse_command(R"(/upload "C:\Users\hitto\My File.txt")");
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->name == "/upload");
    REQUIRE(parsed->args.size() == 1);
    CHECK(parsed->args[0] == R"(C:\Users\hitto\My File.txt)");
}

TEST_CASE("parse_command supports quoted download destination", "[command-parser]") {
    const auto parsed = grotto::parse_command(R"(/download file-123 "downloads/my file.bin")");
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->name == "/download");
    REQUIRE(parsed->args.size() == 2);
    CHECK(parsed->args[0] == "file-123");
    CHECK(parsed->args[1] == "downloads/my file.bin");
}

TEST_CASE("detect_local_file_from_paste accepts quoted existing file path", "[command-parser]") {
    const auto dir = make_temp_dir("quoted");
    const auto path = dir / "my file.txt";
    {
        std::ofstream out(path, std::ios::binary);
        out << "hello";
    }

    const auto detected = grotto::detect_local_file_from_paste("\"" + path.string() + "\"\n");
    REQUIRE(detected.has_value());
    CHECK(*detected == path);

    cleanup(dir);
}

TEST_CASE("detect_local_file_from_paste accepts shell escaped generic path", "[command-parser]") {
    const auto dir = make_temp_dir("escaped");
    const auto path = dir / "drop file.txt";
    {
        std::ofstream out(path, std::ios::binary);
        out << "hello";
    }

    std::string escaped = path.generic_string();
    size_t pos = 0;
    while ((pos = escaped.find(' ', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\ ");
        pos += 2;
    }

    const auto detected = grotto::detect_local_file_from_paste(escaped);
    REQUIRE(detected.has_value());
    CHECK(detected->filename() == path.filename());

    cleanup(dir);
}

TEST_CASE("make_upload_command_for_path quotes file path", "[command-parser]") {
    const std::filesystem::path path = "C:/Users/hitto/drop file.txt";
    CHECK(grotto::make_upload_command_for_path(path) ==
          R"cmd(/upload "C:/Users/hitto/drop file.txt")cmd");
}
