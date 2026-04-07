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

TEST_CASE("file store lists complete DM conversation files in both directions", "[file-store]") {
    const auto db_path = make_temp_db_path();

    {
        grotto::db::Database db(db_path.string());
        grotto::db::FileStore store(db);

        grotto::db::FileMetadata a_to_b;
        a_to_b.file_id = "dm-1";
        a_to_b.filename = "from-alice.txt";
        a_to_b.file_size = 4;
        a_to_b.mime_type = "text/plain";
        a_to_b.sender_id = "alice";
        a_to_b.recipient_id = "bob";
        a_to_b.storage_path = "dm-1";

        grotto::db::FileMetadata b_to_a;
        b_to_a.file_id = "dm-2";
        b_to_a.filename = "from-bob.txt";
        b_to_a.file_size = 4;
        b_to_a.mime_type = "text/plain";
        b_to_a.sender_id = "bob";
        b_to_a.recipient_id = "alice";
        b_to_a.storage_path = "dm-2";

        grotto::db::FileMetadata other;
        other.file_id = "dm-3";
        other.filename = "from-eve.txt";
        other.file_size = 4;
        other.mime_type = "text/plain";
        other.sender_id = "eve";
        other.recipient_id = "alice";
        other.storage_path = "dm-3";

        REQUIRE(store.createFile(a_to_b));
        REQUIRE(store.createFile(b_to_a));
        REQUIRE(store.createFile(other));

        const std::vector<uint8_t> checksum(32, 0x42);
        REQUIRE(store.markComplete(a_to_b.file_id, checksum));
        REQUIRE(store.markComplete(b_to_a.file_id, checksum));

        const auto files = store.listConversationFiles("alice", "bob", 20);
        REQUIRE(files.size() == 2);
        CHECK((files[0].file_id == "dm-1" || files[0].file_id == "dm-2"));
        CHECK((files[1].file_id == "dm-1" || files[1].file_id == "dm-2"));
        CHECK(files[0].file_id != files[1].file_id);
    }

    cleanup(db_path);
}
