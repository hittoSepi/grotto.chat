#include "crypto/file_encryptor.hpp"

#include <openssl/evp.h>
#include <spdlog/spdlog.h>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace grotto::crypto {

namespace {

class EvpCipherCtxGuard {
public:
    EvpCipherCtxGuard()
        : ctx_(EVP_CIPHER_CTX_new()) {
    }

    ~EvpCipherCtxGuard() {
        EVP_CIPHER_CTX_free(ctx_);
    }

    EvpCipherCtxGuard(const EvpCipherCtxGuard&) = delete;
    EvpCipherCtxGuard& operator=(const EvpCipherCtxGuard&) = delete;

    EVP_CIPHER_CTX* get() const { return ctx_; }

private:
    EVP_CIPHER_CTX* ctx_;
};

} // namespace

FileEncryptor::FileEncryptor(const std::string& master_key_hex) {
    auto key_bytes = hex_to_bytes(master_key_hex);
    if (!key_bytes || key_bytes->size() != MASTER_KEY_SIZE) {
        spdlog::error("FileEncryptor: Invalid master key format. Expected {} hex chars.", 
            MASTER_KEY_SIZE * 2);
        return;
    }
    master_key_ = std::move(*key_bytes);
    spdlog::info("FileEncryptor: Initialized with master key");
}

std::vector<uint8_t> FileEncryptor::generate_master_key() {
    std::vector<uint8_t> key(MASTER_KEY_SIZE);
    randombytes_buf(key.data(), key.size());
    return key;
}

std::string FileEncryptor::bytes_to_hex(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : bytes) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

std::optional<std::vector<uint8_t>> FileEncryptor::hex_to_bytes(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        return std::nullopt;
    }
    
    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);
    
    for (size_t i = 0; i < hex.size(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        try {
            int byte = std::stoi(byte_str, nullptr, 16);
            result.push_back(static_cast<uint8_t>(byte));
        } catch (...) {
            return std::nullopt;
        }
    }
    
    return result;
}

std::optional<std::vector<uint8_t>> FileEncryptor::encrypt(
    const std::vector<uint8_t>& plaintext,
    EncryptedKey& out_encrypted_key) {

    if (!is_ready()) {
        spdlog::error("FileEncryptor: Not initialized with master key");
        return std::nullopt;
    }
    
    // Generate random DEK
    std::vector<uint8_t> dek(DEK_SIZE);
    randombytes_buf(dek.data(), dek.size());
    
    // Encrypt the DEK with master key
    auto encrypted_dek = encrypt_dek(dek);
    if (!encrypted_dek) {
        return std::nullopt;
    }
    out_encrypted_key = std::move(*encrypted_dek);
    
    // Generate salt and IV for file encryption
    EncryptedHeader header;
    randombytes_buf(header.salt.data(), header.salt.size());
    randombytes_buf(header.iv.data(), header.iv.size());
    header.original_size = plaintext.size();
    
    // Derive key for file encryption
    auto file_key = derive_key(dek, header.salt);
    if (file_key.empty()) {
        return std::nullopt;
    }

    EvpCipherCtxGuard ctx;
    if (!ctx.get()) {
        spdlog::error("FileEncryptor: Failed to allocate OpenSSL cipher context");
        return std::nullopt;
    }

    std::vector<uint8_t> ciphertext(plaintext.size());

    int update_len = 0;
    int final_len = 0;

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        spdlog::error("FileEncryptor: Failed to initialize AES-256-GCM cipher");
        return std::nullopt;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN,
            static_cast<int>(header.iv.size()), nullptr) != 1) {
        spdlog::error("FileEncryptor: Failed to set AES-256-GCM IV length");
        return std::nullopt;
    }

    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, file_key.data(), header.iv.data()) != 1) {
        spdlog::error("FileEncryptor: Failed to set AES-256-GCM key and IV");
        return std::nullopt;
    }

    if (!plaintext.empty() && EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &update_len,
            plaintext.data(), static_cast<int>(plaintext.size())) != 1) {
        spdlog::error("FileEncryptor: AES-256-GCM encryption failed");
        return std::nullopt;
    }

    if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + update_len, &final_len) != 1) {
        spdlog::error("FileEncryptor: AES-256-GCM finalization failed");
        return std::nullopt;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG,
            static_cast<int>(header.tag.size()), header.tag.data()) != 1) {
        spdlog::error("FileEncryptor: Failed to read AES-256-GCM authentication tag");
        return std::nullopt;
    }

    header.encrypted_size = static_cast<uint64_t>(update_len + final_len);
    ciphertext.resize(static_cast<size_t>(header.encrypted_size));

    std::vector<uint8_t> result;
    result.reserve(SALT_SIZE + IV_SIZE + TAG_SIZE + 16 + ciphertext.size());

    result.insert(result.end(), header.salt.begin(), header.salt.end());
    result.insert(result.end(), header.iv.begin(), header.iv.end());
    result.insert(result.end(), header.tag.begin(), header.tag.end());

    uint8_t size_buf[16];
    std::memcpy(size_buf, &header.original_size, 8);
    std::memcpy(size_buf + 8, &header.encrypted_size, 8);
    result.insert(result.end(), size_buf, size_buf + 16);

    result.insert(result.end(), ciphertext.begin(), ciphertext.end());

    return result;
}

std::optional<std::vector<uint8_t>> FileEncryptor::decrypt(
    const std::vector<uint8_t>& ciphertext_with_header,
    const EncryptedKey& encrypted_key) {

    if (!is_ready()) {
        spdlog::error("FileEncryptor: Not initialized with master key");
        return std::nullopt;
    }
    
    // Minimum size check
    const size_t header_size = SALT_SIZE + IV_SIZE + TAG_SIZE + 16;
    if (ciphertext_with_header.size() < header_size) {
        spdlog::error("FileEncryptor: Input too small for header");
        return std::nullopt;
    }
    
    // Parse header
    EncryptedHeader header;
    size_t pos = 0;

    std::memcpy(header.salt.data(), ciphertext_with_header.data() + pos, SALT_SIZE);
    pos += SALT_SIZE;

    std::memcpy(header.iv.data(), ciphertext_with_header.data() + pos, IV_SIZE);
    pos += IV_SIZE;

    std::memcpy(header.tag.data(), ciphertext_with_header.data() + pos, TAG_SIZE);
    pos += TAG_SIZE;

    std::memcpy(&header.original_size, ciphertext_with_header.data() + pos, 8);
    pos += 8;
    std::memcpy(&header.encrypted_size, ciphertext_with_header.data() + pos, 8);
    pos += 8;

    const size_t payload_size = ciphertext_with_header.size() - header_size;
    if (header.encrypted_size != payload_size) {
        spdlog::error("FileEncryptor: Encrypted payload size mismatch");
        return std::nullopt;
    }
    
    // Decrypt DEK
    auto dek = decrypt_dek(encrypted_key);
    if (!dek) {
        return std::nullopt;
    }
    
    // Derive file key
    auto file_key = derive_key(*dek, header.salt);
    if (file_key.empty()) {
        return std::nullopt;
    }

    EvpCipherCtxGuard ctx;
    if (!ctx.get()) {
        spdlog::error("FileEncryptor: Failed to allocate OpenSSL cipher context");
        return std::nullopt;
    }

    std::vector<uint8_t> plaintext(payload_size);
    int update_len = 0;
    int final_len = 0;

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        spdlog::error("FileEncryptor: Failed to initialize AES-256-GCM decryptor");
        return std::nullopt;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN,
            static_cast<int>(header.iv.size()), nullptr) != 1) {
        spdlog::error("FileEncryptor: Failed to set AES-256-GCM IV length");
        return std::nullopt;
    }

    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, file_key.data(), header.iv.data()) != 1) {
        spdlog::error("FileEncryptor: Failed to set AES-256-GCM key and IV");
        return std::nullopt;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG,
            static_cast<int>(header.tag.size()), const_cast<uint8_t*>(header.tag.data())) != 1) {
        spdlog::error("FileEncryptor: Failed to set AES-256-GCM authentication tag");
        return std::nullopt;
    }

    if (payload_size > 0 && EVP_DecryptUpdate(ctx.get(), plaintext.data(), &update_len,
            ciphertext_with_header.data() + header_size, static_cast<int>(payload_size)) != 1) {
        spdlog::error("FileEncryptor: AES-256-GCM decryption failed");
        return std::nullopt;
    }

    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + update_len, &final_len) != 1) {
        spdlog::error("FileEncryptor: Decryption failed (authentication error)");
        return std::nullopt;
    }

    const size_t plaintext_size = static_cast<size_t>(update_len + final_len);
    if (plaintext_size != header.original_size) {
        spdlog::error("FileEncryptor: Decrypted plaintext size mismatch");
        return std::nullopt;
    }

    plaintext.resize(plaintext_size);

    return plaintext;
}

std::optional<FileEncryptor::EncryptedKey> FileEncryptor::encrypt_dek(
    const std::vector<uint8_t>& dek) {

    EncryptedKey result;
    randombytes_buf(result.iv.data(), result.iv.size());

    // Generate ephemeral salt
    std::array<uint8_t, SALT_SIZE> salt;
    randombytes_buf(salt.data(), salt.size());

    // Derive KEK from master key
    auto kek = derive_key(master_key_, salt);
    if (kek.empty()) {
        return std::nullopt;
    }

    EvpCipherCtxGuard ctx;
    if (!ctx.get()) {
        spdlog::error("FileEncryptor: Failed to allocate OpenSSL cipher context");
        return std::nullopt;
    }

    std::vector<uint8_t> encrypted_dek(dek.size());
    int update_len = 0;
    int final_len = 0;

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        spdlog::error("FileEncryptor: Failed to initialize AES-256-GCM cipher for DEK");
        return std::nullopt;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN,
            static_cast<int>(result.iv.size()), nullptr) != 1) {
        spdlog::error("FileEncryptor: Failed to set AES-256-GCM IV length for DEK");
        return std::nullopt;
    }

    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, kek.data(), result.iv.data()) != 1) {
        spdlog::error("FileEncryptor: Failed to set AES-256-GCM key and IV for DEK");
        return std::nullopt;
    }

    if (!dek.empty() && EVP_EncryptUpdate(ctx.get(), encrypted_dek.data(), &update_len,
            dek.data(), static_cast<int>(dek.size())) != 1) {
        spdlog::error("FileEncryptor: AES-256-GCM DEK encryption failed");
        return std::nullopt;
    }

    if (EVP_EncryptFinal_ex(ctx.get(), encrypted_dek.data() + update_len, &final_len) != 1) {
        spdlog::error("FileEncryptor: AES-256-GCM DEK finalization failed");
        return std::nullopt;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG,
            static_cast<int>(result.tag.size()), result.tag.data()) != 1) {
        spdlog::error("FileEncryptor: Failed to read AES-256-GCM DEK authentication tag");
        return std::nullopt;
    }

    encrypted_dek.resize(static_cast<size_t>(update_len + final_len));

    result.ciphertext.insert(result.ciphertext.begin(), salt.begin(), salt.end());
    result.ciphertext.insert(result.ciphertext.end(), encrypted_dek.begin(), encrypted_dek.end());

    return result;
}

std::optional<std::vector<uint8_t>> FileEncryptor::decrypt_dek(
    const EncryptedKey& encrypted_key) {

    if (encrypted_key.ciphertext.size() < SALT_SIZE + DEK_SIZE) {
        spdlog::error("FileEncryptor: Encrypted DEK too small");
        return std::nullopt;
    }

    // Extract salt
    std::array<uint8_t, SALT_SIZE> salt;
    std::memcpy(salt.data(), encrypted_key.ciphertext.data(), SALT_SIZE);

    // Derive KEK
    auto kek = derive_key(master_key_, salt);
    if (kek.empty()) {
        return std::nullopt;
    }

    const size_t ciphertext_size = encrypted_key.ciphertext.size() - SALT_SIZE;
    EvpCipherCtxGuard ctx;
    if (!ctx.get()) {
        spdlog::error("FileEncryptor: Failed to allocate OpenSSL cipher context");
        return std::nullopt;
    }

    std::vector<uint8_t> dek(ciphertext_size);
    int update_len = 0;
    int final_len = 0;

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        spdlog::error("FileEncryptor: Failed to initialize AES-256-GCM decryptor for DEK");
        return std::nullopt;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN,
            static_cast<int>(encrypted_key.iv.size()), nullptr) != 1) {
        spdlog::error("FileEncryptor: Failed to set AES-256-GCM IV length for DEK");
        return std::nullopt;
    }

    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, kek.data(), encrypted_key.iv.data()) != 1) {
        spdlog::error("FileEncryptor: Failed to set AES-256-GCM key and IV for DEK");
        return std::nullopt;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG,
            static_cast<int>(encrypted_key.tag.size()),
            const_cast<uint8_t*>(encrypted_key.tag.data())) != 1) {
        spdlog::error("FileEncryptor: Failed to set AES-256-GCM DEK authentication tag");
        return std::nullopt;
    }

    if (ciphertext_size > 0 && EVP_DecryptUpdate(ctx.get(), dek.data(), &update_len,
            encrypted_key.ciphertext.data() + SALT_SIZE,
            static_cast<int>(ciphertext_size)) != 1) {
        spdlog::error("FileEncryptor: AES-256-GCM DEK decryption failed");
        return std::nullopt;
    }

    if (EVP_DecryptFinal_ex(ctx.get(), dek.data() + update_len, &final_len) != 1) {
        spdlog::error("FileEncryptor: DEK decryption failed");
        return std::nullopt;
    }

    const size_t dek_size = static_cast<size_t>(update_len + final_len);
    if (dek_size != DEK_SIZE) {
        spdlog::error("FileEncryptor: Decrypted DEK size mismatch");
        return std::nullopt;
    }

    dek.resize(dek_size);

    return dek;
}

std::vector<uint8_t> FileEncryptor::serialize_encrypted_key(const EncryptedKey& key) {
    std::vector<uint8_t> result;
    result.reserve(IV_SIZE + TAG_SIZE + 4 + key.ciphertext.size());
    
    result.insert(result.end(), key.iv.begin(), key.iv.end());
    result.insert(result.end(), key.tag.begin(), key.tag.end());
    
    // Add ciphertext length
    uint32_t len = static_cast<uint32_t>(key.ciphertext.size());
    uint8_t len_buf[4];
    std::memcpy(len_buf, &len, 4);
    result.insert(result.end(), len_buf, len_buf + 4);
    
    result.insert(result.end(), key.ciphertext.begin(), key.ciphertext.end());
    
    return result;
}

std::optional<FileEncryptor::EncryptedKey> FileEncryptor::deserialize_encrypted_key(
    const std::vector<uint8_t>& data) {
    
    const size_t min_size = IV_SIZE + TAG_SIZE + 4;
    if (data.size() < min_size) {
        return std::nullopt;
    }
    
    EncryptedKey result;
    size_t pos = 0;
    
    std::memcpy(result.iv.data(), data.data() + pos, IV_SIZE);
    pos += IV_SIZE;
    
    std::memcpy(result.tag.data(), data.data() + pos, TAG_SIZE);
    pos += TAG_SIZE;
    
    uint32_t len;
    std::memcpy(&len, data.data() + pos, 4);
    pos += 4;
    
    if (data.size() < pos + len) {
        return std::nullopt;
    }
    
    result.ciphertext.assign(data.begin() + pos, data.begin() + pos + len);
    
    return result;
}

std::vector<uint8_t> FileEncryptor::derive_key(
    const std::vector<uint8_t>& master_key,
    const std::array<uint8_t, SALT_SIZE>& salt) {

    std::vector<uint8_t> result(MASTER_KEY_SIZE);  // 32 bytes for AES-256
    
    // Use Argon2id via libsodium's crypto_pwhash
    // This is intentionally slow to deter brute force
    if (crypto_pwhash(result.data(), result.size(),
            reinterpret_cast<const char*>(master_key.data()), master_key.size(),
            salt.data(),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE,
            crypto_pwhash_ALG_ARGON2ID13) != 0) {
        spdlog::error("FileEncryptor: Key derivation failed");
        return {};
    }
    
    return result;
}

void FileEncryptor::random_bytes(void* buf, size_t len) {
    randombytes_buf(buf, len);
}

} // namespace grotto::crypto
