#include <catch2/catch_test_macros.hpp>

#include "utils/channel_utils.hpp"

namespace grotto::utils {

TEST_CASE("sanitize_channel_name adds canonical prefix") {
    CHECK(sanitize_channel_name("general") == "#general");
    CHECK(sanitize_channel_name("#general") == "#general");
    CHECK(sanitize_channel_name("&general") == "#general");
}

TEST_CASE("sanitize_channel_name strips invalid characters") {
    CHECK(sanitize_channel_name("  general chat  ") == "#generalchat");
    CHECK(sanitize_channel_name("##dev!") == "#dev");
    CHECK(sanitize_channel_name("+") == "#");
}

TEST_CASE("is_valid_channel_name requires content after prefix") {
    CHECK(is_valid_channel_name("#general"));
    CHECK_FALSE(is_valid_channel_name("#"));
    CHECK_FALSE(is_valid_channel_name("general"));
    CHECK_FALSE(is_valid_channel_name(""));
}

} // namespace grotto::utils
