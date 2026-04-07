#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <sodium.h>

namespace grotto::utils {

inline std::vector<uint8_t> sha256_bytes(std::span<const uint8_t> data) {
    std::vector<uint8_t> digest(crypto_hash_sha256_BYTES);
    crypto_hash_sha256(
        digest.data(),
        data.empty() ? nullptr : data.data(),
        static_cast<unsigned long long>(data.size()));
    return digest;
}

inline bool checksum_matches(std::span<const uint8_t> data, std::span<const uint8_t> expected) {
    if (expected.empty()) {
        return true;
    }
    const auto actual = sha256_bytes(data);
    return actual.size() == expected.size() &&
           std::equal(actual.begin(), actual.end(), expected.begin());
}

inline bool checksum_matches(std::span<const uint8_t> data, const std::string& expected) {
    return checksum_matches(data, std::span<const uint8_t>(
                                      reinterpret_cast<const uint8_t*>(expected.data()),
                                      expected.size()));
}

} // namespace grotto::utils
