#include "config.hpp"
#include "crypto/crypto_engine.hpp"
#include "db/local_store.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sodium.h>

#include <chrono>
#include <filesystem>
#include <system_error>

namespace {

std::filesystem::path make_temp_dir(const char* tag) {
    const auto tick = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      (std::string("grotto-") + tag + "-" + std::to_string(tick));
    std::filesystem::create_directories(path);
    return path;
}

void cleanup(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

grotto::ClientConfig make_cfg(const std::filesystem::path& dir, const std::string& user_id) {
    grotto::ClientConfig cfg;
    cfg.identity.user_id = user_id;
    cfg.config_dir = dir;
    cfg.db_path = dir / "client.db";
    return cfg;
}

KeyBundle make_bundle_for(const grotto::crypto::CryptoEngine& recipient,
                          const KeyUpload& upload,
                          const std::string& recipient_id) {
    KeyBundle bundle;
    const auto identity_pub = recipient.identity_pub();
    const auto spk = recipient.current_spk();

    bundle.set_identity_pub(identity_pub.data(), identity_pub.size());
    bundle.set_signed_prekey(upload.signed_prekey());
    bundle.set_spk_signature(upload.spk_signature());
    bundle.set_spk_id(spk.id);
    if (upload.one_time_prekeys_size() > 0) {
        bundle.set_one_time_prekey(upload.one_time_prekeys(0));
        bundle.set_opk_id(upload.opk_ids(0));
    }
    bundle.set_registration_id(upload.registration_id());
    bundle.set_recipient_for(recipient_id);
    return bundle;
}

} // namespace

TEST_CASE("CryptoEngine pre-key roundtrip decrypts across two fresh clients", "[signal][prekey]") {
    REQUIRE(sodium_init() >= 0);

    const auto alice_dir = make_temp_dir("signal-alice");
    const auto bob_dir = make_temp_dir("signal-bob");

    auto cleanup_all = [&]() {
        cleanup(alice_dir);
        cleanup(bob_dir);
    };

    grotto::db::LocalStore alice_store(alice_dir / "client.db");
    grotto::db::LocalStore bob_store(bob_dir / "client.db");

    auto alice_cfg = make_cfg(alice_dir, "alice");
    auto bob_cfg = make_cfg(bob_dir, "bob");

    grotto::crypto::CryptoEngine alice;
    grotto::crypto::CryptoEngine bob;

    REQUIRE(alice.init(alice_store, alice_cfg, "passphrase"));
    REQUIRE(bob.init(bob_store, bob_cfg, "passphrase"));

    auto bob_upload = bob.prepare_key_upload(1);
    auto alice_upload = alice.prepare_key_upload(1);

    KeyBundle bundle_for_bob = make_bundle_for(bob, bob_upload, "bob");
    KeyBundle bundle_for_alice = make_bundle_for(alice, alice_upload, "alice");

    REQUIRE(alice.on_key_bundle(bundle_for_bob, "bob"));
    ChatEnvelope a_to_b = alice.encrypt("alice", "bob", "hello bob", [](const std::string&) {});
    REQUIRE(!a_to_b.ciphertext().empty());
    REQUIRE(a_to_b.ciphertext_type() == 3);

    auto bob_result = bob.decrypt(a_to_b);
    INFO("bob decrypt success=" << bob_result.success << " sender=" << bob_result.sender_id
         << " plaintext=" << bob_result.plaintext);
    REQUIRE(bob_result.success);
    REQUIRE(bob_result.sender_id == "alice");
    REQUIRE(bob_result.plaintext == "hello bob");

    REQUIRE(bob.on_key_bundle(bundle_for_alice, "alice"));
    ChatEnvelope b_to_a = bob.encrypt("bob", "alice", "hello alice", [](const std::string&) {});
    REQUIRE(!b_to_a.ciphertext().empty());
    REQUIRE(b_to_a.ciphertext_type() == 3);

    auto alice_result = alice.decrypt(b_to_a);
    INFO("alice decrypt success=" << alice_result.success << " sender=" << alice_result.sender_id
         << " plaintext=" << alice_result.plaintext);
    REQUIRE(alice_result.success);
    REQUIRE(alice_result.sender_id == "bob");
    REQUIRE(alice_result.plaintext == "hello alice");

    cleanup_all();
}
