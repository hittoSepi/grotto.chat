#include <catch2/catch_test_macros.hpp>

#include "utils/checksum_utils.hpp"

#include <sodium.h>

#include <vector>

TEST_CASE("checksum utils round-trip payload digests", "[checksum]") {
    REQUIRE(sodium_init() >= 0);

    const std::vector<uint8_t> payload = {'g', 'r', 'o', 't', 't', 'o'};
    const auto checksum = grotto::utils::sha256_bytes(payload);

    REQUIRE(checksum.size() == crypto_hash_sha256_BYTES);
    CHECK(grotto::utils::checksum_matches(payload, checksum));

    auto corrupted = checksum;
    corrupted.front() ^= 0xFF;
    CHECK_FALSE(grotto::utils::checksum_matches(payload, corrupted));
}
