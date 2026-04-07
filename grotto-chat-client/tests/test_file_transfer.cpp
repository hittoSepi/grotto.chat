#include <catch2/catch_test_macros.hpp>

#include "file/file_transfer.hpp"

#include <sodium.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <system_error>
#include <vector>

namespace {

std::filesystem::path make_temp_dir(const char* tag) {
    const auto tick = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      (std::string("grotto-file-transfer-") + tag + "-" + std::to_string(tick));
    std::filesystem::create_directories(path);
    return path;
}

void cleanup(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

std::vector<uint8_t> sha256_bytes(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> digest(crypto_hash_sha256_BYTES);
    crypto_hash_sha256(digest.data(), data.data(), data.size());
    return digest;
}

bool matches_bytes(const std::string& protobuf_bytes, const std::vector<uint8_t>& expected) {
    const auto* actual = reinterpret_cast<const uint8_t*>(protobuf_bytes.data());
    return protobuf_bytes.size() == expected.size() &&
           std::equal(expected.begin(), expected.end(), actual);
}

} // namespace

TEST_CASE("upload emits per-chunk sha256 checksums", "[file-transfer]") {
    REQUIRE(sodium_init() >= 0);

    const auto dir = make_temp_dir("upload");
    const auto path = dir / "payload.bin";
    const std::vector<uint8_t> payload = {'h', 'e', 'l', 'l', 'o', '-', 'g', 'r', 'o', 't', 't', 'o'};
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    }

    grotto::client::file::FileTransferManager manager;
    std::mutex mu;
    std::condition_variable cv;
    bool received_chunk = false;
    FileUploadChunk captured_chunk;

    manager.set_send_function([&](uint32_t, const google::protobuf::Message& msg) {
        if (msg.GetTypeName() != "FileUploadChunk") {
            return;
        }
        std::lock_guard<std::mutex> lock(mu);
        captured_chunk.CopyFrom(static_cast<const FileUploadChunk&>(msg));
        received_chunk = true;
        cv.notify_all();
    });

    const auto transfer_id = manager.upload(path, "bob", "");
    REQUIRE_FALSE(transfer_id.empty());

    std::unique_lock<std::mutex> lock(mu);
    REQUIRE(cv.wait_for(lock, std::chrono::seconds(5), [&] { return received_chunk; }));

    const auto expected = sha256_bytes(payload);
    REQUIRE(captured_chunk.data() == std::string(reinterpret_cast<const char*>(payload.data()), payload.size()));
    REQUIRE(captured_chunk.checksum().size() == expected.size());
    CHECK(matches_bytes(captured_chunk.checksum(), expected));

    Envelope probe;
    probe.set_type(MT_FILE_UPLOAD);
    probe.set_timestamp_ms(1);
    probe.set_payload(captured_chunk.SerializeAsString());
    CHECK(probe.ByteSizeLong() <= 65536);

    cleanup(dir);
}

TEST_CASE("detect_mime_type infers common file types", "[file-transfer]") {
    CHECK(grotto::client::file::detect_mime_type("photo.JPG") == "image/jpeg");
    CHECK(grotto::client::file::detect_mime_type("note.txt") == "text/plain");
    CHECK(grotto::client::file::detect_mime_type("blob.bin") == "application/octet-stream");
}

TEST_CASE("download completion verifies matching checksum", "[file-transfer]") {
    REQUIRE(sodium_init() >= 0);

    const auto dir = make_temp_dir("download-ok");
    const auto path = dir / "download.bin";
    const std::vector<uint8_t> payload = {'o', 'k', '-', 'p', 'a', 'y', 'l', 'o', 'a', 'd'};

    grotto::client::file::FileTransferManager manager;
    manager.set_send_function([](uint32_t, const google::protobuf::Message&) {});

    const auto transfer_id = manager.download("file-ok", path);
    REQUIRE_FALSE(transfer_id.empty());

    grotto::client::file::FileChunk chunk;
    chunk.file_id = "file-ok";
    chunk.chunk_index = 0;
    chunk.data = payload;
    chunk.is_last = true;
    manager.on_file_chunk(chunk);

    FileComplete complete;
    complete.set_file_id("file-ok");
    complete.set_total_bytes(payload.size());
    const auto checksum = sha256_bytes(payload);
    complete.set_file_checksum(checksum.data(), checksum.size());
    manager.on_file_complete(complete);

    const auto info = manager.get_transfer(transfer_id);
    REQUIRE(info.has_value());
    CHECK(info->state == grotto::client::file::TransferState::COMPLETED);
    CHECK(info->error_message.empty());

    cleanup(dir);
}

TEST_CASE("download completion rejects checksum mismatch", "[file-transfer]") {
    REQUIRE(sodium_init() >= 0);

    const auto dir = make_temp_dir("download-bad");
    const auto path = dir / "download.bin";
    const std::vector<uint8_t> payload = {'b', 'a', 'd', '-', 'p', 'a', 'y', 'l', 'o', 'a', 'd'};

    grotto::client::file::FileTransferManager manager;
    manager.set_send_function([](uint32_t, const google::protobuf::Message&) {});

    const auto transfer_id = manager.download("file-bad", path);
    REQUIRE_FALSE(transfer_id.empty());

    grotto::client::file::FileChunk chunk;
    chunk.file_id = "file-bad";
    chunk.chunk_index = 0;
    chunk.data = payload;
    chunk.is_last = true;
    manager.on_file_chunk(chunk);

    FileComplete complete;
    complete.set_file_id("file-bad");
    complete.set_total_bytes(payload.size());
    const auto checksum = sha256_bytes(std::vector<uint8_t>{'n', 'o', 'p', 'e'});
    complete.set_file_checksum(checksum.data(), checksum.size());
    manager.on_file_complete(complete);

    const auto info = manager.get_transfer(transfer_id);
    REQUIRE(info.has_value());
    CHECK(info->state == grotto::client::file::TransferState::FAILED);
    CHECK(info->error_message == "Checksum mismatch after transfer");

    cleanup(dir);
}

TEST_CASE("list_transfers returns newest updates first", "[file-transfer]") {
    REQUIRE(sodium_init() >= 0);

    const auto dir = make_temp_dir("history");
    const auto first_path = dir / "first.bin";
    const auto second_path = dir / "second.bin";
    {
        std::ofstream out(first_path, std::ios::binary);
        out << "first";
    }
    {
        std::ofstream out(second_path, std::ios::binary);
        out << "second";
    }

    grotto::client::file::FileTransferManager manager;
    manager.set_send_function([](uint32_t, const google::protobuf::Message&) {});

    const auto first_id = manager.upload(first_path, "bob", "");
    const auto second_id = manager.download("file-second", second_path);
    REQUIRE_FALSE(first_id.empty());
    REQUIRE_FALSE(second_id.empty());

    FileError error;
    error.set_file_id("file-second");
    error.set_error_message("quota exceeded");
    manager.on_file_error(error);

    const auto transfers = manager.list_transfers();
    REQUIRE(transfers.size() >= 2);
    CHECK(transfers[0].updated_at_ms >= transfers[1].updated_at_ms);

    cleanup(dir);
}
