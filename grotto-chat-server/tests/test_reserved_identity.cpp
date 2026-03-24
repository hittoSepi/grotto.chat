#include <catch2/catch_test_macros.hpp>

#include "admin/reserved_identity.hpp"

#include <vector>

namespace grotto {

TEST_CASE("reserved identity exact and case-insensitive checks") {
    CHECK(ReservedIdentity::is_reserved("admin"));
    CHECK(ReservedIdentity::is_reserved("Admin"));
    CHECK(ReservedIdentity::is_reserved("root"));
    CHECK(ReservedIdentity::is_reserved("grotto"));
    CHECK_FALSE(ReservedIdentity::is_reserved("randomuser123"));
}

TEST_CASE("reserved identity handles leet variants") {
    CHECK(ReservedIdentity::is_reserved("4dmin"));
    CHECK(ReservedIdentity::is_reserved("adm1n"));
    CHECK(ReservedIdentity::normalize_leet("4dm1n") == "admin");
}

TEST_CASE("normalize strips special characters and lowercases") {
    CHECK(ReservedIdentity::normalize("Ad-min_123!!") == "admin123");
}

TEST_CASE("owner detection matches configured owner id") {
    CHECK(ReservedIdentity::is_owner("admin"));
    CHECK_FALSE(ReservedIdentity::is_owner("user1"));
}

TEST_CASE("exact reserved list contains expected values") {
    const auto& reserved = ReservedIdentity::get_exact_reserved();

    CHECK(reserved.contains("admin"));
    CHECK(reserved.contains("root"));
    CHECK(reserved.contains("grotto"));
}

TEST_CASE("additional config patterns are applied") {
    const std::vector<std::string> additional_patterns = {"^customowner$", "^staff.*"};

    CHECK(ReservedIdentity::is_reserved("Custom-Owner", additional_patterns));
    CHECK(ReservedIdentity::is_reserved("staffLead", additional_patterns));
    CHECK_FALSE(ReservedIdentity::is_reserved("member", additional_patterns));
}

} // namespace grotto
