#include <catch2/catch_test_macros.hpp>

#include "crypto/file_encryptor.hpp"

#include <sodium.h>

#include <string>
#include <vector>

namespace grotto::crypto {
namespace {

void ensure_sodium_initialized() {
    REQUIRE(sodium_init() >= 0);
}

std::vector<uint8_t> to_bytes(const std::string& value) {
    return std::vector<uint8_t>(value.begin(), value.end());
}

} // namespace

TEST_CASE("generate_master_key returns 32 bytes") {
    ensure_sodium_initialized();

    const auto key = FileEncryptor::generate_master_key();

    REQUIRE(key.size() == FileEncryptor::MASTER_KEY_SIZE);
}

TEST_CASE("hex conversion round-trips and rejects invalid input") {
    ensure_sodium_initialized();

    const std::vector<uint8_t> original = {0x00, 0x12, 0xab, 0xcd, 0xef};
    const auto hex = FileEncryptor::bytes_to_hex(original);
    const auto round_trip = FileEncryptor::hex_to_bytes(hex);

    REQUIRE(round_trip.has_value());
    CHECK(*round_trip == original);
    CHECK_FALSE(FileEncryptor::hex_to_bytes("abc").has_value());
    CHECK_FALSE(FileEncryptor::hex_to_bytes("zz").has_value());
}

TEST_CASE("encrypt and decrypt round-trip plaintext") {
    ensure_sodium_initialized();

    const auto master_key = FileEncryptor::generate_master_key();
    const auto master_key_hex = FileEncryptor::bytes_to_hex(master_key);
    FileEncryptor encryptor(master_key_hex);
    FileEncryptor::EncryptedKey encrypted_key;
    const auto plaintext = to_bytes("hello encrypted world");

    const auto ciphertext = encryptor.encrypt(plaintext, encrypted_key);

    REQUIRE(ciphertext.has_value());

    const auto decrypted = encryptor.decrypt(*ciphertext, encrypted_key);

    REQUIRE(decrypted.has_value());
    CHECK(*decrypted == plaintext);
}

TEST_CASE("encrypt returns nullopt when encryptor is not initialized") {
    ensure_sodium_initialized();

    FileEncryptor encryptor("not-a-valid-master-key");
    FileEncryptor::EncryptedKey encrypted_key;

    const auto ciphertext = encryptor.encrypt(to_bytes("plaintext"), encrypted_key);

    CHECK_FALSE(ciphertext.has_value());
}

TEST_CASE("decrypt fails with wrong master key") {
    ensure_sodium_initialized();

    const auto master_key = FileEncryptor::generate_master_key();
    const auto other_key = FileEncryptor::generate_master_key();
    FileEncryptor encryptor(FileEncryptor::bytes_to_hex(master_key));
    FileEncryptor wrong_encryptor(FileEncryptor::bytes_to_hex(other_key));
    FileEncryptor::EncryptedKey encrypted_key;
    const auto plaintext = to_bytes("top secret message");

    const auto ciphertext = encryptor.encrypt(plaintext, encrypted_key);
    REQUIRE(ciphertext.has_value());

    const auto decrypted = wrong_encryptor.decrypt(*ciphertext, encrypted_key);

    CHECK_FALSE(decrypted.has_value());
}

TEST_CASE("encrypt_dek and decrypt_dek round-trip") {
    ensure_sodium_initialized();

    const auto master_key = FileEncryptor::generate_master_key();
    FileEncryptor encryptor(FileEncryptor::bytes_to_hex(master_key));
    const auto dek = FileEncryptor::generate_master_key();

    const auto encrypted_dek = encryptor.encrypt_dek(dek);

    REQUIRE(encrypted_dek.has_value());

    const auto decrypted_dek = encryptor.decrypt_dek(*encrypted_dek);

    REQUIRE(decrypted_dek.has_value());
    CHECK(*decrypted_dek == dek);
}

TEST_CASE("serialize and deserialize encrypted key round-trip") {
    ensure_sodium_initialized();

    const auto master_key = FileEncryptor::generate_master_key();
    FileEncryptor encryptor(FileEncryptor::bytes_to_hex(master_key));
    const auto dek = FileEncryptor::generate_master_key();
    const auto encrypted_dek = encryptor.encrypt_dek(dek);

    REQUIRE(encrypted_dek.has_value());

    const auto serialized = FileEncryptor::serialize_encrypted_key(*encrypted_dek);
    const auto deserialized = FileEncryptor::deserialize_encrypted_key(serialized);

    REQUIRE(deserialized.has_value());
    CHECK(deserialized->ciphertext == encrypted_dek->ciphertext);
    CHECK(deserialized->iv == encrypted_dek->iv);
    CHECK(deserialized->tag == encrypted_dek->tag);
}

TEST_CASE("different encryptions of same plaintext produce different ciphertext") {
    ensure_sodium_initialized();

    const auto master_key = FileEncryptor::generate_master_key();
    FileEncryptor encryptor(FileEncryptor::bytes_to_hex(master_key));
    FileEncryptor::EncryptedKey first_key;
    FileEncryptor::EncryptedKey second_key;
    const auto plaintext = to_bytes("same plaintext every time");

    const auto first_ciphertext = encryptor.encrypt(plaintext, first_key);
    const auto second_ciphertext = encryptor.encrypt(plaintext, second_key);

    REQUIRE(first_ciphertext.has_value());
    REQUIRE(second_ciphertext.has_value());
    CHECK(*first_ciphertext != *second_ciphertext);
}

} // namespace grotto::crypto
