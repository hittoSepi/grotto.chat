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

TEST_CASE("file store reports reserved bytes for server and user quotas", "[file-store]") {
    const auto db_path = make_temp_db_path();

    {
        grotto::db::Database db(db_path.string());
        grotto::db::FileStore store(db);

        grotto::db::FileMetadata alice_active;
        alice_active.file_id = "quota-1";
        alice_active.filename = "alice-active.bin";
        alice_active.file_size = 100;
        alice_active.mime_type = "application/octet-stream";
        alice_active.sender_id = "alice";
        alice_active.recipient_id = "bob";
        alice_active.storage_path = "quota-1";

        grotto::db::FileMetadata alice_incomplete;
        alice_incomplete.file_id = "quota-2";
        alice_incomplete.filename = "alice-incomplete.bin";
        alice_incomplete.file_size = 50;
        alice_incomplete.mime_type = "application/octet-stream";
        alice_incomplete.sender_id = "alice";
        alice_incomplete.recipient_id = "bob";
        alice_incomplete.storage_path = "quota-2";

        grotto::db::FileMetadata bob_active;
        bob_active.file_id = "quota-3";
        bob_active.filename = "bob-active.bin";
        bob_active.file_size = 25;
        bob_active.mime_type = "application/octet-stream";
        bob_active.sender_id = "bob";
        bob_active.recipient_id = "alice";
        bob_active.storage_path = "quota-3";

        grotto::db::FileMetadata expired;
        expired.file_id = "quota-4";
        expired.filename = "expired.bin";
        expired.file_size = 1000;
        expired.mime_type = "application/octet-stream";
        expired.sender_id = "alice";
        expired.recipient_id = "bob";
        expired.storage_path = "quota-4";
        expired.expires_at = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() - 1;

        REQUIRE(store.createFile(alice_active));
        REQUIRE(store.createFile(alice_incomplete));
        REQUIRE(store.createFile(bob_active));
        REQUIRE(store.createFile(expired));

        CHECK(store.getReservedBytes() == 175);
        CHECK(store.getUserReservedBytes("alice") == 150);
        CHECK(store.getUserReservedBytes("bob") == 25);
        CHECK(store.getUserReservedBytes("nobody") == 0);
    }

    cleanup(db_path);
}
