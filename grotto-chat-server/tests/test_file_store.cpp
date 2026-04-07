#include <catch2/catch_test_macros.hpp>

#include "db/database.hpp"
#include "db/file_store.hpp"
#include "utils/checksum_utils.hpp"

#include <chrono>
#include <filesystem>
#include <span>
#include <system_error>
#include <vector>

namespace {

std::filesystem::path make_temp_db_path() {
    const auto tick = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("grotto-file-store-" + std::to_string(tick) + ".sqlite3");
}

void cleanup(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

} // namespace

TEST_CASE("file store persists completion checksum", "[file-store]") {
    const auto db_path = make_temp_db_path();

    {
        grotto::db::Database db(db_path.string());
        grotto::db::FileStore store(db);

        grotto::db::FileMetadata metadata;
        metadata.file_id = "file-1";
        metadata.filename = "payload.bin";
        metadata.file_size = 4;
        metadata.mime_type = "application/octet-stream";
        metadata.sender_id = "alice";
        metadata.recipient_id = "bob";
        metadata.storage_path = "payload.bin";

        REQUIRE(store.createFile(metadata));
        REQUIRE(store.storeChunk(metadata.file_id, 0, std::vector<uint8_t>{1, 2, 3, 4}));

        const std::vector<uint8_t> payload = {1, 2, 3, 4};
        const auto checksum = grotto::utils::sha256_bytes(std::span<const uint8_t>(payload.data(), payload.size()));
        REQUIRE(store.markComplete(metadata.file_id, checksum));

        const auto stored = store.getFile(metadata.file_id);
        REQUIRE(stored.has_value());
        CHECK(stored->is_complete);
        CHECK(stored->file_checksum == checksum);
    }

    cleanup(db_path);
}
