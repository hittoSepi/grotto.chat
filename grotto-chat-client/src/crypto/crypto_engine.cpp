#include "crypto/crypto_engine.hpp"
#include <sodium.h>
#include <signal_protocol.h>
#include <curve.h>
#include <session_builder.h>
#include <session_cipher.h>
#include <key_helper.h>
#include <session_pre_key.h>
#include <protocol.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <chrono>
#include <algorithm>
#include <cstring>

// libsignal-protocol-c requires a crypto provider backed by libsodium
extern "C" {
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
}

namespace grotto::crypto {

namespace {

bool store_spk_record(signal_context* signal_ctx,
                      db::LocalStore* local_store,
                      const Identity::SignedPreKey& spk) {
    if (!signal_ctx || !local_store) return false;

    std::array<uint8_t, 33> spk_prefixed{};
    spk_prefixed[0] = 0x05;
    std::memcpy(spk_prefixed.data() + 1, spk.key_pair.pub.data(), 32);

    ec_public_key* spk_pub_key = nullptr;
    ec_private_key* spk_priv_key = nullptr;
    ec_key_pair* spk_key_pair = nullptr;
    session_signed_pre_key* spk_record = nullptr;
    signal_buffer* spk_serialized = nullptr;

    bool ok =
        curve_decode_point(&spk_pub_key, spk_prefixed.data(), spk_prefixed.size(), signal_ctx) == 0 &&
        curve_decode_private_point(&spk_priv_key, spk.key_pair.priv.data(), spk.key_pair.priv.size(), signal_ctx) == 0 &&
        ec_key_pair_create(&spk_key_pair, spk_pub_key, spk_priv_key) == 0 &&
        session_signed_pre_key_create(&spk_record, spk.id,
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()),
            spk_key_pair, spk.signature.data(), spk.signature.size()) == 0 &&
        session_signed_pre_key_serialize(&spk_serialized, spk_record) == 0;

    if (ok) {
        local_store->store_signed_pre_key(spk.id,
            std::vector<uint8_t>(signal_buffer_data(spk_serialized),
                                 signal_buffer_data(spk_serialized) + signal_buffer_len(spk_serialized)));
    }

    if (spk_serialized) signal_buffer_free(spk_serialized);
    if (spk_record) SIGNAL_UNREF(spk_record);
    if (spk_key_pair) SIGNAL_UNREF(spk_key_pair);
    if (spk_priv_key) SIGNAL_UNREF(spk_priv_key);
    if (spk_pub_key) SIGNAL_UNREF(spk_pub_key);
    return ok;
}

bool verify_spk_signature(signal_context* signal_ctx,
                          const std::array<uint8_t, 32>& identity_ed25519_pub,
                          const Identity::SignedPreKey& spk) {
    if (!signal_ctx || spk.signature.empty()) return false;

    std::array<uint8_t, 32> x25519_identity_pub{};
    if (crypto_sign_ed25519_pk_to_curve25519(x25519_identity_pub.data(),
                                             identity_ed25519_pub.data()) != 0) {
        return false;
    }

    std::array<uint8_t, 33> identity_prefixed{};
    identity_prefixed[0] = 0x05;
    std::memcpy(identity_prefixed.data() + 1, x25519_identity_pub.data(), 32);

    std::array<uint8_t, 33> spk_prefixed{};
    spk_prefixed[0] = 0x05;
    std::memcpy(spk_prefixed.data() + 1, spk.key_pair.pub.data(), 32);

    ec_public_key* identity_key = nullptr;
    ec_public_key* spk_key = nullptr;
    signal_buffer* serialized_spk = nullptr;

    int rc = curve_decode_point(&identity_key, identity_prefixed.data(), identity_prefixed.size(), signal_ctx);
    if (rc == 0) {
        rc = curve_decode_point(&spk_key, spk_prefixed.data(), spk_prefixed.size(), signal_ctx);
    }
    if (rc == 0) {
        rc = ec_public_key_serialize(&serialized_spk, spk_key);
    }
    if (rc != 0) {
        if (serialized_spk) signal_buffer_free(serialized_spk);
        if (spk_key) SIGNAL_UNREF(spk_key);
        if (identity_key) SIGNAL_UNREF(identity_key);
        return false;
    }

    rc = curve_verify_signature(identity_key,
        signal_buffer_data(serialized_spk),
        signal_buffer_len(serialized_spk),
        spk.signature.data(),
        spk.signature.size());

    signal_buffer_free(serialized_spk);
    SIGNAL_UNREF(spk_key);
    SIGNAL_UNREF(identity_key);
    return rc == 1;
}

std::vector<uint8_t> extract_public_key_no_prefix(ec_public_key* public_key) {
    signal_buffer* serialized = nullptr;
    if (!public_key || ec_public_key_serialize(&serialized, public_key) != 0 || !serialized) {
        return {};
    }

    const uint8_t* data = signal_buffer_data(serialized);
    const size_t len = signal_buffer_len(serialized);
    std::vector<uint8_t> result;
    if (len > 1 && data[0] == 0x05) {
        result.assign(data + 1, data + len);
    } else {
        result.assign(data, data + len);
    }
    signal_buffer_free(serialized);
    return result;
}

std::vector<uint8_t> load_local_signed_pre_key_public(db::LocalStore* local_store,
                                                      signal_context* signal_ctx,
                                                      uint32_t id) {
    if (!local_store || !signal_ctx) return {};
    auto data = local_store->load_signed_pre_key(id);
    if (!data) return {};

    session_signed_pre_key* record = nullptr;
    if (session_signed_pre_key_deserialize(&record, data->data(), data->size(), signal_ctx) != 0 || !record) {
        return {};
    }

    ec_key_pair* key_pair = session_signed_pre_key_get_key_pair(record);
    std::vector<uint8_t> result = extract_public_key_no_prefix(
        key_pair ? ec_key_pair_get_public(key_pair) : nullptr);
    SIGNAL_UNREF(record);
    return result;
}

std::vector<uint8_t> load_local_pre_key_public(db::LocalStore* local_store,
                                               signal_context* signal_ctx,
                                               uint32_t id) {
    if (!local_store || !signal_ctx) return {};
    auto data = local_store->load_pre_key(id);
    if (!data) return {};

    session_pre_key* record = nullptr;
    if (session_pre_key_deserialize(&record, data->data(), data->size(), signal_ctx) != 0 || !record) {
        return {};
    }

    ec_key_pair* key_pair = session_pre_key_get_key_pair(record);
    std::vector<uint8_t> result = extract_public_key_no_prefix(
        key_pair ? ec_key_pair_get_public(key_pair) : nullptr);
    SIGNAL_UNREF(record);
    return result;
}

std::string short_hex(const std::vector<uint8_t>& data, size_t max_bytes = 8) {
    if (data.empty()) return "(empty)";
    static constexpr char kHex[] = "0123456789abcdef";
    const size_t n = std::min(max_bytes, data.size());
    std::string out;
    out.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        uint8_t b = data[i];
        out.push_back(kHex[b >> 4]);
        out.push_back(kHex[b & 0x0f]);
    }
    return out;
}

} // namespace

// ── libsignal-protocol-c crypto provider (OpenSSL-backed) ────────────────────

static int signal_random_bytes(uint8_t* data, size_t len, void*) {
    randombytes_buf(data, len);
    return SG_SUCCESS;
}

static int signal_hmac_sha256_init(void** ctx, const uint8_t* key, size_t key_len, void*) {
    EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    if (!mac) return SG_ERR_NOMEM;

    EVP_MAC_CTX* mac_ctx = EVP_MAC_CTX_new(mac);
    EVP_MAC_free(mac);
    if (!mac_ctx) return SG_ERR_NOMEM;

    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                         const_cast<char*>("SHA256"), 0),
        OSSL_PARAM_construct_end(),
    };

    if (EVP_MAC_init(mac_ctx, key, key_len, params) != 1) {
        EVP_MAC_CTX_free(mac_ctx);
        return SG_ERR_UNKNOWN;
    }

    *ctx = mac_ctx;
    return SG_SUCCESS;
}

static int signal_hmac_sha256_update(void* ctx, const uint8_t* data, size_t len, void*) {
    return EVP_MAC_update(static_cast<EVP_MAC_CTX*>(ctx), data, len) == 1
        ? SG_SUCCESS
        : SG_ERR_UNKNOWN;
}

static int signal_hmac_sha256_final(void* ctx, signal_buffer** out, void*) {
    auto* mac_ctx = static_cast<EVP_MAC_CTX*>(ctx);
    uint8_t buf[EVP_MAX_MD_SIZE];
    size_t len = sizeof(buf);
    if (EVP_MAC_final(mac_ctx, buf, &len, sizeof(buf)) != 1) {
        return SG_ERR_UNKNOWN;
    }
    *out = signal_buffer_create(buf, len);
    return *out ? SG_SUCCESS : SG_ERR_NOMEM;
}

static void signal_hmac_sha256_cleanup(void* ctx, void*) {
    EVP_MAC_CTX_free(static_cast<EVP_MAC_CTX*>(ctx));
}

static int signal_sha512_digest_init(void** ctx, void*) {
    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) return SG_ERR_NOMEM;
    if (EVP_DigestInit_ex(md_ctx, EVP_sha512(), nullptr) != 1) {
        EVP_MD_CTX_free(md_ctx);
        return SG_ERR_UNKNOWN;
    }
    *ctx = md_ctx;
    return SG_SUCCESS;
}

static int signal_sha512_digest_update(void* ctx, const uint8_t* data, size_t len, void*) {
    return EVP_DigestUpdate(static_cast<EVP_MD_CTX*>(ctx), data, len) == 1
        ? SG_SUCCESS
        : SG_ERR_UNKNOWN;
}

static int signal_sha512_digest_final(void* ctx, signal_buffer** out, void*) {
    auto* md_ctx = static_cast<EVP_MD_CTX*>(ctx);
    uint8_t buf[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    if (EVP_DigestFinal_ex(md_ctx, buf, &len) != 1) {
        return SG_ERR_UNKNOWN;
    }
    *out = signal_buffer_create(buf, len);
    return *out ? SG_SUCCESS : SG_ERR_NOMEM;
}

static void signal_sha512_digest_cleanup(void* ctx, void*) {
    EVP_MD_CTX_free(static_cast<EVP_MD_CTX*>(ctx));
}

// AES-CBC encrypt/decrypt via OpenSSL
static int signal_encrypt(signal_buffer** out,
                           int cipher,
                           const uint8_t* key, size_t key_len,
                           const uint8_t* iv, size_t /*iv_len*/,
                           const uint8_t* data, size_t data_len,
                           void*)
{
    const EVP_CIPHER* evp = nullptr;
    switch (cipher) {
    case SG_CIPHER_AES_CBC_PKCS5:
        evp = (key_len == 16) ? EVP_aes_128_cbc() : EVP_aes_256_cbc(); break;
    case SG_CIPHER_AES_CTR_NOPADDING:
        evp = (key_len == 16) ? EVP_aes_128_ctr() : EVP_aes_256_ctr(); break;
    default: return SG_ERR_UNKNOWN;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, evp, nullptr, key, iv);
    if (cipher == SG_CIPHER_AES_CTR_NOPADDING) EVP_CIPHER_CTX_set_padding(ctx, 0);

    std::vector<uint8_t> buf(data_len + EVP_CIPHER_block_size(evp));
    int len1 = 0, len2 = 0;
    EVP_EncryptUpdate(ctx, buf.data(), &len1, data, static_cast<int>(data_len));
    EVP_EncryptFinal_ex(ctx, buf.data() + len1, &len2);
    EVP_CIPHER_CTX_free(ctx);

    *out = signal_buffer_create(buf.data(), len1 + len2);
    return *out ? SG_SUCCESS : SG_ERR_NOMEM;
}

static int signal_decrypt(signal_buffer** out,
                           int cipher,
                           const uint8_t* key, size_t key_len,
                           const uint8_t* iv, size_t /*iv_len*/,
                           const uint8_t* data, size_t data_len,
                           void*)
{
    const EVP_CIPHER* evp = nullptr;
    switch (cipher) {
    case SG_CIPHER_AES_CBC_PKCS5:
        evp = (key_len == 16) ? EVP_aes_128_cbc() : EVP_aes_256_cbc(); break;
    case SG_CIPHER_AES_CTR_NOPADDING:
        evp = (key_len == 16) ? EVP_aes_128_ctr() : EVP_aes_256_ctr(); break;
    default: return SG_ERR_UNKNOWN;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, evp, nullptr, key, iv);
    if (cipher == SG_CIPHER_AES_CTR_NOPADDING) EVP_CIPHER_CTX_set_padding(ctx, 0);

    std::vector<uint8_t> buf(data_len + EVP_CIPHER_block_size(evp));
    int len1 = 0, len2 = 0;
    EVP_DecryptUpdate(ctx, buf.data(), &len1, data, static_cast<int>(data_len));
    EVP_DecryptFinal_ex(ctx, buf.data() + len1, &len2);
    EVP_CIPHER_CTX_free(ctx);

    *out = signal_buffer_create(buf.data(), len1 + len2);
    return *out ? SG_SUCCESS : SG_ERR_NOMEM;
}

// ── CryptoEngine ──────────────────────────────────────────────────────────────

CryptoEngine::CryptoEngine() = default;

CryptoEngine::~CryptoEngine() {
    if (store_ctx_) {
        signal_protocol_store_context_destroy(store_ctx_);
        store_ctx_ = nullptr;
    }
    if (signal_ctx_) {
        signal_context_destroy(signal_ctx_);
        signal_ctx_ = nullptr;
    }
}

void CryptoEngine::setup_signal_crypto(signal_context* ctx) {
    signal_crypto_provider provider{};
    provider.random_func               = signal_random_bytes;
    provider.hmac_sha256_init_func     = signal_hmac_sha256_init;
    provider.hmac_sha256_update_func   = signal_hmac_sha256_update;
    provider.hmac_sha256_final_func    = signal_hmac_sha256_final;
    provider.hmac_sha256_cleanup_func  = signal_hmac_sha256_cleanup;
    provider.sha512_digest_init_func   = signal_sha512_digest_init;
    provider.sha512_digest_update_func = signal_sha512_digest_update;
    provider.sha512_digest_final_func  = signal_sha512_digest_final;
    provider.sha512_digest_cleanup_func= signal_sha512_digest_cleanup;
    provider.encrypt_func              = signal_encrypt;
    provider.decrypt_func              = signal_decrypt;
    signal_context_set_crypto_provider(ctx, &provider);
}

bool CryptoEngine::init(db::LocalStore& store, const ClientConfig& cfg,
                         const std::string& passphrase) {
    local_store_ = &store;
    const std::string& user_id = cfg.identity.user_id;

    // Init libsignal context
    if (signal_context_create(&signal_ctx_, nullptr) != SG_SUCCESS) {
        spdlog::error("Failed to create signal context");
        return false;
    }
    setup_signal_crypto(signal_ctx_);
    signal_context_set_log_function(signal_ctx_, nullptr);

    // Store context
    if (signal_protocol_store_context_create(&store_ctx_, signal_ctx_) != SG_SUCCESS) {
        spdlog::error("Failed to create signal store context");
        return false;
    }

    // Set up store adapters
    signal_store_ = std::make_unique<SignalStore>(store, user_id);
    signal_store_->register_with_context(store_ctx_);

    // Load or generate identity
    bool has_id = store.has_identity(user_id);
    if (has_id) {
        if (!identity_.load(user_id, passphrase, store)) {
            spdlog::error("Failed to load identity — wrong passphrase?");
            return false;
        }
    } else {
        if (!identity_.generate_and_save(user_id, passphrase, store)) {
            spdlog::error("Failed to generate identity");
            return false;
        }
    }

    // Set identity key in signal store (needed for group sessions)
    const auto& ed_keys = identity_.ed25519();
    signal_store_->set_identity_key(ed_keys.pub, ed_keys.priv);

    // Load persisted SPK if available; otherwise generate once and persist it.
    if (auto persisted_spk = local_store_->load_local_signed_pre_key_state();
        persisted_spk &&
        persisted_spk->pub.size() == spk_.key_pair.pub.size() &&
        persisted_spk->priv.size() == spk_.key_pair.priv.size() &&
        !persisted_spk->signature.empty()) {
        spk_.id = persisted_spk->id;
        std::copy(persisted_spk->pub.begin(), persisted_spk->pub.end(), spk_.key_pair.pub.begin());
        std::copy(persisted_spk->priv.begin(), persisted_spk->priv.end(), spk_.key_pair.priv.begin());
        spk_.signature = persisted_spk->signature;

        if (verify_spk_signature(signal_ctx_, ed_keys.pub, spk_)) {
            if (!store_spk_record(signal_ctx_, local_store_, spk_)) {
                spdlog::error("Failed to restore persisted signed pre-key id={}", spk_.id);
                return false;
            }

            spdlog::info("Loaded persisted signed pre-key id={}", spk_.id);
        } else {
            spdlog::warn("Persisted signed pre-key id={} has invalid signature; regenerating", spk_.id);
            local_store_->remove_signed_pre_key(spk_.id);
            local_store_->clear_local_signed_pre_key_state();
            spk_ = {};
        }
    }

    if (spk_.signature.empty()) {
        // Generate SPK using libsignal-compatible XEdDSA signing
        spk_.id = 1;
        if (crypto_box_keypair(spk_.key_pair.pub.data(), spk_.key_pair.priv.data()) != 0) {
            spdlog::error("Failed to generate X25519 SPK");
            return false;
        }

        // Convert Ed25519 identity private key to Curve25519 for libsignal signing
        std::array<uint8_t, 32> x25519_identity_priv{};
        if (crypto_sign_ed25519_sk_to_curve25519(x25519_identity_priv.data(), ed_keys.priv.data()) != 0) {
            spdlog::error("Failed to convert Ed25519 identity key to Curve25519");
            return false;
        }

        ec_private_key* identity_priv_key = nullptr;
        if (curve_decode_private_point(&identity_priv_key, x25519_identity_priv.data(),
                                       x25519_identity_priv.size(), signal_ctx_) != 0) {
            spdlog::error("Failed to decode identity private key for SPK signing");
            return false;
        }

        // Build ec_public_key for the SPK from raw 32-byte pub + 0x05 prefix
        std::array<uint8_t, 33> spk_prefixed{};
        spk_prefixed[0] = 0x05;
        std::memcpy(spk_prefixed.data() + 1, spk_.key_pair.pub.data(), 32);

        ec_public_key* spk_pub_key = nullptr;
        if (curve_decode_point(&spk_pub_key, spk_prefixed.data(), spk_prefixed.size(), signal_ctx_) != 0) {
            spdlog::error("Failed to decode SPK public key for signing");
            SIGNAL_UNREF(identity_priv_key);
            return false;
        }

        // Serialize the SPK public key to get the message to sign (33 bytes with 0x05 prefix)
        signal_buffer* spk_serialized = nullptr;
        if (ec_public_key_serialize(&spk_serialized, spk_pub_key) != 0) {
            spdlog::error("Failed to serialize SPK public key for signing");
            SIGNAL_UNREF(identity_priv_key);
            SIGNAL_UNREF(spk_pub_key);
            return false;
        }

        // Sign using libsignal's curve_calculate_signature (XEdDSA)
        signal_buffer* sig_buf = nullptr;
        if (curve_calculate_signature(signal_ctx_, &sig_buf, identity_priv_key,
                signal_buffer_data(spk_serialized), signal_buffer_len(spk_serialized)) != 0) {
            spdlog::error("Failed to sign SPK with libsignal");
            signal_buffer_free(spk_serialized);
            SIGNAL_UNREF(identity_priv_key);
            SIGNAL_UNREF(spk_pub_key);
            return false;
        }

        spk_.signature.assign(signal_buffer_data(sig_buf),
                              signal_buffer_data(sig_buf) + signal_buffer_len(sig_buf));

        signal_buffer_free(sig_buf);
        signal_buffer_free(spk_serialized);
        SIGNAL_UNREF(identity_priv_key);
        SIGNAL_UNREF(spk_pub_key);

        if (!store_spk_record(signal_ctx_, local_store_, spk_)) {
            spdlog::error("Failed to serialize/store local signed pre-key");
            return false;
        }

        db::LocalStore::LocalSignedPreKeyState state;
        state.id = spk_.id;
        state.pub.assign(spk_.key_pair.pub.begin(), spk_.key_pair.pub.end());
        state.priv.assign(spk_.key_pair.priv.begin(), spk_.key_pair.priv.end());
        state.signature = spk_.signature;
        local_store_->store_local_signed_pre_key_state(state);
        spdlog::info("Generated and persisted signed pre-key id={}", spk_.id);
    } else {
        spdlog::info("Using validated persisted signed pre-key id={}", spk_.id);
    }
    // Load persisted counter to avoid reusing pre-key IDs after restart
    next_opk_id_ = local_store_->load_pre_key_counter();
    if (next_opk_id_ == 0) next_opk_id_ = 1;

    // Group session
    group_session_ = std::make_unique<GroupSession>(store_ctx_, signal_ctx_);
    group_session_->set_local_identity(user_id);

    spdlog::info("CryptoEngine initialized for '{}'", user_id);
    return true;
}

KeyUpload CryptoEngine::prepare_key_upload(int num_opks) {
    KeyUpload ku;
    ku.set_signed_prekey(spk_.key_pair.pub.data(), spk_.key_pair.pub.size());
    ku.set_spk_signature(spk_.signature.data(), spk_.signature.size());
    ku.set_spk_id(spk_.id);

    auto opks = identity_.generate_one_time_prekeys(next_opk_id_, num_opks);
    for (auto& [id, kp] : opks) {
        ku.add_one_time_prekeys(kp.pub.data(), kp.pub.size());
        ku.add_opk_ids(id);

        // Store OPK in libsignal serialized format so incoming PreKeySignalMessages
        // can load the matching private key during X3DH.
        std::array<uint8_t, 33> opk_prefixed{};
        opk_prefixed[0] = 0x05;
        std::memcpy(opk_prefixed.data() + 1, kp.pub.data(), 32);

        ec_public_key*  opk_pub_key  = nullptr;
        ec_private_key* opk_priv_key = nullptr;
        ec_key_pair*    opk_key_pair = nullptr;
        session_pre_key* opk_record  = nullptr;
        signal_buffer*   opk_serialized = nullptr;

        if (curve_decode_point(&opk_pub_key, opk_prefixed.data(), opk_prefixed.size(), signal_ctx_) == 0 &&
            curve_decode_private_point(&opk_priv_key, kp.priv.data(), kp.priv.size(), signal_ctx_) == 0 &&
            ec_key_pair_create(&opk_key_pair, opk_pub_key, opk_priv_key) == 0 &&
            session_pre_key_create(&opk_record, id, opk_key_pair) == 0 &&
            session_pre_key_serialize(&opk_serialized, opk_record) == 0) {
            local_store_->store_pre_key(id,
                std::vector<uint8_t>(signal_buffer_data(opk_serialized),
                                     signal_buffer_data(opk_serialized) + signal_buffer_len(opk_serialized)));
        } else {
            spdlog::error("Failed to serialize/store one-time pre-key id={}", id);
        }

        if (opk_serialized) signal_buffer_free(opk_serialized);
        if (opk_record) SIGNAL_UNREF(opk_record);
        if (opk_key_pair) SIGNAL_UNREF(opk_key_pair);
        if (opk_priv_key) SIGNAL_UNREF(opk_priv_key);
        if (opk_pub_key) SIGNAL_UNREF(opk_pub_key);
    }
    next_opk_id_ += num_opks;
    local_store_->store_pre_key_counter(next_opk_id_);
    return ku;
}

std::vector<uint8_t> CryptoEngine::sign_challenge(const std::vector<uint8_t>& nonce,
                                                    const std::string& user_id) {
    return identity_.sign_challenge(nonce, user_id);
}

std::vector<uint8_t> CryptoEngine::identity_pub() const {
    return {identity_.ed25519().pub.begin(), identity_.ed25519().pub.end()};
}

CryptoEngine::SpkInfo CryptoEngine::current_spk() const {
    return {
        {spk_.key_pair.pub.begin(), spk_.key_pair.pub.end()},
        spk_.signature,
        spk_.id
    };
}

void CryptoEngine::reset_dm_session(const std::string& peer_id) {
    if (peer_id.empty() || !local_store_) return;
    local_store_->delete_session(peer_id, 1);
    pending_plaintexts_.erase(peer_id);
    spdlog::info("Reset DM session with '{}'", peer_id);
}

void CryptoEngine::reset_all_dm_sessions() {
    if (!local_store_) return;
    local_store_->delete_all_sessions();
    pending_plaintexts_.clear();
    spdlog::info("Reset all DM sessions");
}

// ── Encrypt ───────────────────────────────────────────────────────────────────

ChatEnvelope CryptoEngine::encrypt(const std::string& sender_id,
                                    const std::string& recipient_id,
                                    const std::string& plaintext,
                                    std::function<void(const std::string&)> key_request_fn) {
    ChatEnvelope env;
    env.set_sender_id(sender_id);
    env.set_recipient_id(recipient_id);

    const std::vector<uint8_t> plaintext_bytes(plaintext.begin(), plaintext.end());

    // ── Group channel (#channel): use Sender Key, no key bundle request ───
    if (!recipient_id.empty() && recipient_id[0] == '#') {
        bool need_skdm = (sent_skdm_channels_.find(recipient_id) == sent_skdm_channels_.end());

        if (need_skdm) {
            // First message to this channel this session — create fresh group
            // session and include SKDM so all receivers can decrypt.
            try {
                auto skdm = group_session_->create_session(recipient_id);
                auto ct   = group_session_->encrypt(recipient_id, plaintext_bytes);
                env.set_ciphertext(ct.data(), ct.size());
                env.set_ciphertext_type(4);
                env.set_skdm(skdm.data(), skdm.size());
                sent_skdm_channels_.insert(recipient_id);
                spdlog::debug("Group encrypt: created session for {}, SKDM size: {} bytes",
                              recipient_id, skdm.size());
                return env;
            } catch (const std::exception& e) {
                spdlog::error("Group encrypt failed for {}: {}", recipient_id, e.what());
                return {};
            }
        }

        try {
            auto ct = group_session_->encrypt(recipient_id, plaintext_bytes);
            env.set_ciphertext(ct.data(), ct.size());
            env.set_ciphertext_type(4);  // SENDER_KEY_MESSAGE
            return env;
        } catch (const std::exception& e) {
            spdlog::error("Group encrypt failed for {}: {} — resetting session, next message will re-send SKDM", recipient_id, e.what());
            // Drop the broken session so the next send creates a fresh one with SKDM
            sent_skdm_channels_.erase(recipient_id);
            return {};
        }
    }

    // ── DM (user_id): use 1:1 Signal session ─────────────────────────────
    signal_protocol_address addr{};
    addr.name       = recipient_id.c_str();
    addr.name_len   = recipient_id.size();
    addr.device_id  = 1;

    bool has_session = signal_protocol_session_contains_session(store_ctx_, &addr) == 1;

    if (!has_session) {
        // Queue and request key bundle
        pending_plaintexts_[recipient_id] = plaintext;
        key_request_fn(recipient_id);
        // Return empty envelope (caller should not send yet)
        return {};
    }

    session_cipher* cipher = nullptr;
    int rc = session_cipher_create(&cipher, store_ctx_, &addr, signal_ctx_);
    if (rc != SG_SUCCESS) {
        spdlog::error("session_cipher_create failed: {}", rc);
        return {};
    }

    ciphertext_message* encrypted = nullptr;
    rc = session_cipher_encrypt(cipher,
        reinterpret_cast<const uint8_t*>(plaintext.data()), plaintext.size(),
        &encrypted);
    session_cipher_free(cipher);

    if (rc != SG_SUCCESS) {
        spdlog::error("session_cipher_encrypt failed: {}", rc);
        reset_dm_session(recipient_id);
        return {};
    }

    signal_buffer* buf = ciphertext_message_get_serialized(encrypted);
    env.set_ciphertext(signal_buffer_data(buf), signal_buffer_len(buf));
    env.set_ciphertext_type(ciphertext_message_get_type(encrypted));

    if (env.ciphertext_type() == 3) {
        pre_key_signal_message* msg = nullptr;
        int parse_rc = pre_key_signal_message_deserialize(
            &msg,
            reinterpret_cast<const uint8_t*>(env.ciphertext().data()),
            env.ciphertext().size(),
            signal_ctx_);
        if (parse_rc == SG_SUCCESS) {
            const bool has_pre_key_id = pre_key_signal_message_has_pre_key_id(msg) == 1;
            spdlog::debug(
                "Outgoing PRE_KEY message to '{}': version={} registration_id={} signed_pre_key_id={} has_pre_key_id={} pre_key_id={}",
                recipient_id,
                static_cast<unsigned>(pre_key_signal_message_get_message_version(msg)),
                pre_key_signal_message_get_registration_id(msg),
                pre_key_signal_message_get_signed_pre_key_id(msg),
                has_pre_key_id,
                has_pre_key_id ? pre_key_signal_message_get_pre_key_id(msg) : 0);
            SIGNAL_UNREF(msg);
        } else {
            spdlog::warn("Failed to deserialize outgoing PRE_KEY message for {}: {}", recipient_id, parse_rc);
        }
    }

    SIGNAL_UNREF(encrypted);

    return env;
}

// ── Decrypt ───────────────────────────────────────────────────────────────────

DecryptResult CryptoEngine::decrypt(const ChatEnvelope& env) {
    DecryptResult result;
    result.sender_id = env.sender_id();

    const auto& ct_str = env.ciphertext();
    const uint8_t* ct  = reinterpret_cast<const uint8_t*>(ct_str.data());
    size_t ct_len       = ct_str.size();

    // ── Group channel (Sender Key) ────────────────────────────────────────
    if (env.ciphertext_type() == 4) {
        const std::string& channel_id = env.recipient_id();

        // Process SKDM if present (first message from this sender in channel).
        // This establishes the sender key state needed for decryption.
        if (!env.skdm().empty()) {
            const auto& skdm_str = env.skdm();
            group_session_->process_sender_key_distribution(
                channel_id, env.sender_id(), 1 /*device_id*/,
                std::vector<uint8_t>(skdm_str.begin(), skdm_str.end()));
        }

        try {
            auto pt = group_session_->decrypt(
                channel_id, env.sender_id(), 1 /*device_id*/,
                std::vector<uint8_t>(ct, ct + ct_len));
            result.plaintext.assign(pt.begin(), pt.end());
            result.success = true;
        } catch (const std::exception& e) {
            spdlog::warn("Group decrypt failed ({}/{}): {}",
                         channel_id, env.sender_id(), e.what());
        }
        return result;
    }

    // ── DM (1:1 Signal session) ───────────────────────────────────────────
    signal_protocol_address addr{};
    addr.name      = env.sender_id().c_str();
    addr.name_len  = env.sender_id().size();
    addr.device_id = 1;

    session_cipher* cipher = nullptr;
    int rc = session_cipher_create(&cipher, store_ctx_, &addr, signal_ctx_);
    if (rc != SG_SUCCESS) {
        spdlog::warn("session_cipher_create failed for {}: {}", env.sender_id(), rc);
        return result;
    }

    signal_buffer* plaintext_buf = nullptr;

    bool had_session_before_decrypt = signal_protocol_session_contains_session(store_ctx_, &addr) == 1;
    spdlog::debug("Decrypting DM from '{}' type={} had_session={} current_spk_id={}",
                  env.sender_id(), env.ciphertext_type(), had_session_before_decrypt, spk_.id);

    if (env.ciphertext_type() == 3) {  // PRE_KEY_SIGNAL_MESSAGE
        pre_key_signal_message* msg = nullptr;
        rc = pre_key_signal_message_deserialize(&msg, ct, ct_len, signal_ctx_);
        if (rc == SG_SUCCESS) {
            const bool has_pre_key_id = pre_key_signal_message_has_pre_key_id(msg) == 1;
            const uint32_t pre_key_id = has_pre_key_id ? pre_key_signal_message_get_pre_key_id(msg) : 0;
            const uint32_t signed_pre_key_id = pre_key_signal_message_get_signed_pre_key_id(msg);
            spdlog::debug("PRE_KEY message from '{}' local_spk_present={} local_prekey_counter_next={}",
                          env.sender_id(),
                          local_store_ && local_store_->contains_signed_pre_key(spk_.id),
                          next_opk_id_);
            spdlog::debug(
                "PRE_KEY metadata from '{}': version={} registration_id={} signed_pre_key_id={} local_signed_pre_key_present={} has_pre_key_id={} pre_key_id={} local_pre_key_present={}",
                env.sender_id(),
                static_cast<unsigned>(pre_key_signal_message_get_message_version(msg)),
                pre_key_signal_message_get_registration_id(msg),
                signed_pre_key_id,
                local_store_ && local_store_->contains_signed_pre_key(signed_pre_key_id),
                has_pre_key_id,
                pre_key_id,
                has_pre_key_id && local_store_ && local_store_->contains_pre_key(pre_key_id));

            auto local_spk_pub = load_local_signed_pre_key_public(local_store_, signal_ctx_, signed_pre_key_id);
            auto local_opk_pub = has_pre_key_id ? load_local_pre_key_public(local_store_, signal_ctx_, pre_key_id)
                                                : std::vector<uint8_t>{};
            spdlog::debug(
                "PRE_KEY local record state from '{}': signed_pre_key_matches_current={} local_spk_pub_size={} local_pre_key_loaded={} local_pre_key_pub_size={}",
                env.sender_id(),
                !local_spk_pub.empty() && local_spk_pub == std::vector<uint8_t>(spk_.key_pair.pub.begin(), spk_.key_pair.pub.end()),
                local_spk_pub.size(),
                has_pre_key_id && !local_opk_pub.empty(),
                local_opk_pub.size());
            spdlog::debug(
                "PRE_KEY local pub fingerprints from '{}': current_spk={} local_spk={} local_pre_key={}",
                env.sender_id(),
                short_hex(std::vector<uint8_t>(spk_.key_pair.pub.begin(), spk_.key_pair.pub.end())),
                short_hex(local_spk_pub),
                short_hex(local_opk_pub));
            rc = session_cipher_decrypt_pre_key_signal_message(cipher, msg, nullptr, &plaintext_buf);
            SIGNAL_UNREF(msg);
        }
    } else {  // SIGNAL_MESSAGE
        signal_message* msg = nullptr;
        rc = signal_message_deserialize(&msg, ct, ct_len, signal_ctx_);
        if (rc == SG_SUCCESS) {
            rc = session_cipher_decrypt_signal_message(cipher, msg, nullptr, &plaintext_buf);
            SIGNAL_UNREF(msg);
        }
    }

    session_cipher_free(cipher);

    if (rc != SG_SUCCESS || !plaintext_buf) {
        bool has_session_after_decrypt = signal_protocol_session_contains_session(store_ctx_, &addr) == 1;
        spdlog::warn("Decryption failed for message from {}: rc={} type={} had_session_before={} has_session_after={} local_spk_id={}",
                     env.sender_id(), rc, env.ciphertext_type(),
                     had_session_before_decrypt, has_session_after_decrypt, spk_.id);
        reset_dm_session(env.sender_id());
        return result;
    }

    result.plaintext.assign(
        reinterpret_cast<const char*>(signal_buffer_data(plaintext_buf)),
        signal_buffer_len(plaintext_buf));
    signal_buffer_free(plaintext_buf);
    result.success = true;
    return result;
}

// ── Key bundle handling ───────────────────────────────────────────────────────

bool CryptoEngine::on_key_bundle(const KeyBundle& bundle, const std::string& recipient_id) {
    // Build session_pre_key_bundle from the KeyBundle proto.
    // libsignal's curve_decode_point expects 33-byte keys with a 0x05 prefix.
    // The identity_pub sent by the server is Ed25519 (32 bytes), so we convert
    // it to Curve25519 first. signed_prekey and one_time_prekey are already
    // X25519 but also need the 0x05 prefix.
    ec_public_key* identity_key   = nullptr;
    ec_public_key* signed_pre_key = nullptr;
    ec_public_key* one_time_key   = nullptr;

    const auto& id_pub_str = bundle.identity_pub();
    const auto& spk_str    = bundle.signed_prekey();
    const auto& opk_str    = bundle.one_time_prekey();

    // --- identity key: Ed25519 -> Curve25519 + 0x05 prefix ---
    if (id_pub_str.size() == 32) {
        std::array<uint8_t, 32> x25519_pub{};
        if (crypto_sign_ed25519_pk_to_curve25519(x25519_pub.data(),
            reinterpret_cast<const uint8_t*>(id_pub_str.data())) == 0) {
            std::array<uint8_t, 33> prefixed{};
            prefixed[0] = 0x05;
            std::memcpy(prefixed.data() + 1, x25519_pub.data(), 32);
            int rc = curve_decode_point(&identity_key, prefixed.data(), prefixed.size(), signal_ctx_);
            if (rc != 0) {
                spdlog::error("curve_decode_point failed for identity key of {}: {}", recipient_id, rc);
            }
        } else {
            spdlog::error("Ed25519->Curve25519 conversion failed for identity key of {}", recipient_id);
        }
    } else {
        spdlog::error("Unexpected identity_pub size for {}: {} (expected 32)", recipient_id, id_pub_str.size());
    }

    // --- signed pre-key: add 0x05 prefix ---
    if (!spk_str.empty()) {
        std::vector<uint8_t> prefixed;
        prefixed.reserve(spk_str.size() + 1);
        prefixed.push_back(0x05);
        prefixed.insert(prefixed.end(), spk_str.begin(), spk_str.end());
        int rc = curve_decode_point(&signed_pre_key, prefixed.data(), prefixed.size(), signal_ctx_);
        if (rc != 0) {
            spdlog::error("curve_decode_point failed for signed pre-key of {}: {}", recipient_id, rc);
        }
    }

    // --- one-time pre-key: add 0x05 prefix ---
    if (!opk_str.empty()) {
        std::vector<uint8_t> prefixed;
        prefixed.reserve(opk_str.size() + 1);
        prefixed.push_back(0x05);
        prefixed.insert(prefixed.end(), opk_str.begin(), opk_str.end());
        int rc = curve_decode_point(&one_time_key, prefixed.data(), prefixed.size(), signal_ctx_);
        if (rc != 0) {
            spdlog::error("curve_decode_point failed for one-time pre-key of {}: {}", recipient_id, rc);
        }
    }

    if (!identity_key || !signed_pre_key) {
        spdlog::error("Missing required keys in KeyBundle for {} (identity={}, signed_pre_key={})",
            recipient_id, identity_key != nullptr, signed_pre_key != nullptr);
        SIGNAL_UNREF(identity_key);
        SIGNAL_UNREF(signed_pre_key);
        if (one_time_key) SIGNAL_UNREF(one_time_key);
        return false;
    }

    const auto& spk_sig = bundle.spk_signature();
    spdlog::debug(
        "KeyBundle for '{}': identity_pub_size={} signed_prekey_size={} spk_sig_size={} spk_id={} opk_present={} opk_id={}",
        recipient_id,
        id_pub_str.size(),
        spk_str.size(),
        spk_sig.size(),
        bundle.spk_id(),
        !opk_str.empty(),
        bundle.opk_id());
    spdlog::debug(
        "KeyBundle pub fingerprints for '{}': signed_prekey={} one_time_prekey={}",
        recipient_id,
        short_hex(std::vector<uint8_t>(spk_str.begin(), spk_str.end())),
        short_hex(std::vector<uint8_t>(opk_str.begin(), opk_str.end())));

    signal_buffer* serialized_signed_pre_key = nullptr;
    int verify_rc = ec_public_key_serialize(&serialized_signed_pre_key, signed_pre_key);
    if (verify_rc == SG_SUCCESS) {
        verify_rc = curve_verify_signature(
            identity_key,
            signal_buffer_data(serialized_signed_pre_key),
            signal_buffer_len(serialized_signed_pre_key),
            reinterpret_cast<const uint8_t*>(spk_sig.data()),
            spk_sig.size());
        spdlog::debug(
            "KeyBundle signature check for '{}': verify_rc={} serialized_spk_size={}",
            recipient_id,
            verify_rc,
            signal_buffer_len(serialized_signed_pre_key));
    } else {
        spdlog::error("Failed to serialize signed pre-key from bundle for {}: {}", recipient_id, verify_rc);
    }
    if (serialized_signed_pre_key) {
        signal_buffer_free(serialized_signed_pre_key);
    }

    session_pre_key_bundle* pkb = nullptr;
    int rc = session_pre_key_bundle_create(&pkb,
        1,  // registration_id
        1,  // device_id
        bundle.opk_id(), one_time_key,
        bundle.spk_id(), signed_pre_key,
        reinterpret_cast<const uint8_t*>(spk_sig.data()), spk_sig.size(),
        identity_key);

    if (rc != SG_SUCCESS) {
        spdlog::error("session_pre_key_bundle_create failed: {}", rc);
        SIGNAL_UNREF(identity_key);
        SIGNAL_UNREF(signed_pre_key);
        if (one_time_key) SIGNAL_UNREF(one_time_key);
        return false;
    }

    signal_protocol_address addr{};
    addr.name      = recipient_id.c_str();
    addr.name_len  = recipient_id.size();
    addr.device_id = 1;

    if (signal_protocol_session_contains_session(store_ctx_, &addr) == 1) {
        spdlog::debug("Dropping existing DM session with '{}' before processing new KeyBundle", recipient_id);
        reset_dm_session(recipient_id);
    }

    session_builder* builder = nullptr;
    rc = session_builder_create(&builder, store_ctx_, &addr, signal_ctx_);
    if (rc == SG_SUCCESS) {
        rc = session_builder_process_pre_key_bundle(builder, pkb);
        session_builder_free(builder);
    }

    if (rc == SG_ERR_UNTRUSTED_IDENTITY && local_store_) {
        // Stale/wrong identity key in local DB — clear it and retry once (re-TOFU)
        spdlog::warn("Untrusted identity for {} — clearing stale key and retrying", recipient_id);
        local_store_->delete_peer_identity(recipient_id);

        session_builder* builder2 = nullptr;
        rc = session_builder_create(&builder2, store_ctx_, &addr, signal_ctx_);
        if (rc == SG_SUCCESS) {
            rc = session_builder_process_pre_key_bundle(builder2, pkb);
            session_builder_free(builder2);
        }
    }

    SIGNAL_UNREF(pkb);
    SIGNAL_UNREF(identity_key);
    SIGNAL_UNREF(signed_pre_key);
    if (one_time_key) SIGNAL_UNREF(one_time_key);

    if (rc != SG_SUCCESS) {
        spdlog::error("session_builder_process_pre_key_bundle failed for {}: {}", recipient_id, rc);
        return false;
    }

    spdlog::info("Established X3DH session with '{}'", recipient_id);
    return true;
}

std::string CryptoEngine::safety_number(const std::string& peer_id, db::LocalStore& store) {
    auto peer_pub = store.load_peer_identity(peer_id);
    if (!peer_pub || peer_pub->size() != 32) {
        return "(no key on file for " + peer_id + ")";
    }
    std::array<uint8_t, 32> their_pub{};
    std::copy(peer_pub->begin(), peer_pub->end(), their_pub.begin());
    return Identity::safety_number(identity_.ed25519().pub, their_pub);
}

} // namespace grotto::crypto
