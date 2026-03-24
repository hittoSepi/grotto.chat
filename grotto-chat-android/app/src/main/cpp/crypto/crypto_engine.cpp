#include "crypto/crypto_engine.hpp"
#include "crypto/group_session.hpp"
#include <android/log.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <cstring>
#include <ctime>

// Signal Protocol headers
#include <signal_protocol.h>
#include <key_helper.h>
#include <protocol.h>
#include <session_builder.h>
#include <session_cipher.h>
#include <session_pre_key.h>
#include <session_state.h>
#include <curve.h>
#include <hkdf.h>
#include <ratchet.h>
#include <group_session_builder.h>
#include <group_cipher.h>

// Signal Protocol compatibility
#ifndef SIGNAL_UNREF
#define SIGNAL_UNREF(obj) signal_type_unref(obj)
#endif

#define LOG_TAG "GrottoCrypto"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace grotto::crypto {

// ============================================================================
// Global Signal Protocol Context
// ============================================================================

static signal_context* g_signal_ctx = nullptr;
static signal_protocol_store_context* g_store_ctx = nullptr;

// Crypto provider callbacks for libsignal-protocol-c
static int random_bytes(uint8_t* data, size_t len, void* user_data) {
    (void)user_data;
    if (RAND_bytes(data, static_cast<int>(len)) != 1) {
        return SG_ERR_UNKNOWN;
    }
    return 0;
}

static int hmac_sha256_init(void** hmac_context, const uint8_t* key, size_t key_len, void* user_data) {
    (void)user_data;
    EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    if (!mac) return SG_ERR_UNKNOWN;
    
    EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
    EVP_MAC_free(mac);
    if (!ctx) return SG_ERR_UNKNOWN;
    
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>("SHA256"), 0),
        OSSL_PARAM_construct_end()
    };
    
    if (EVP_MAC_init(ctx, key, key_len, params) != 1) {
        EVP_MAC_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }
    
    *hmac_context = ctx;
    return 0;
}

static int hmac_sha256_update(void* hmac_context, const uint8_t* data, size_t data_len, void* user_data) {
    (void)user_data;
    if (EVP_MAC_update(static_cast<EVP_MAC_CTX*>(hmac_context), data, data_len) != 1) {
        return SG_ERR_UNKNOWN;
    }
    return 0;
}

static int hmac_sha256_final(void* hmac_context, signal_buffer** output, void* user_data) {
    (void)user_data;
    size_t out_len = 0;
    if (EVP_MAC_final(static_cast<EVP_MAC_CTX*>(hmac_context), nullptr, &out_len, 0) != 1) {
        return SG_ERR_UNKNOWN;
    }
    
    signal_buffer* buf = signal_buffer_alloc(out_len);
    if (!buf) return SG_ERR_NOMEM;
    
    if (EVP_MAC_final(static_cast<EVP_MAC_CTX*>(hmac_context), signal_buffer_data(buf), &out_len, out_len) != 1) {
        signal_buffer_free(buf);
        return SG_ERR_UNKNOWN;
    }
    
    *output = buf;
    return 0;
}

static void hmac_sha256_cleanup(void* hmac_context, void* user_data) {
    (void)user_data;
    EVP_MAC_CTX_free(static_cast<EVP_MAC_CTX*>(hmac_context));
}

// SHA512 digest helper (used internally for safety numbers etc.)
static int sha512_digest_helper(signal_buffer** output, const uint8_t* data, size_t data_len) {
    signal_buffer* buf = signal_buffer_alloc(64);
    if (!buf) return SG_ERR_NOMEM;

    if (EVP_Digest(data, data_len, signal_buffer_data(buf), nullptr, EVP_sha512(), nullptr) != 1) {
        signal_buffer_free(buf);
        return SG_ERR_UNKNOWN;
    }

    *output = buf;
    return 0;
}

// SHA256 digest helper (used internally)
static int sha256_digest_helper(signal_buffer** output, const uint8_t* data, size_t data_len) {
    signal_buffer* buf = signal_buffer_alloc(32);
    if (!buf) return SG_ERR_NOMEM;

    if (EVP_Digest(data, data_len, signal_buffer_data(buf), nullptr, EVP_sha256(), nullptr) != 1) {
        signal_buffer_free(buf);
        return SG_ERR_UNKNOWN;
    }

    *output = buf;
    return 0;
}

// SHA512 init/update/final/cleanup callbacks for signal_crypto_provider
static int sha512_digest_init(void** digest_context, void* user_data) {
    (void)user_data;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return SG_ERR_NOMEM;
    if (EVP_DigestInit_ex(ctx, EVP_sha512(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }
    *digest_context = ctx;
    return 0;
}

static int sha512_digest_update(void* digest_context, const uint8_t* data, size_t data_len, void* user_data) {
    (void)user_data;
    if (EVP_DigestUpdate(static_cast<EVP_MD_CTX*>(digest_context), data, data_len) != 1) {
        return SG_ERR_UNKNOWN;
    }
    return 0;
}

static int sha512_digest_final(void* digest_context, signal_buffer** output, void* user_data) {
    (void)user_data;
    signal_buffer* buf = signal_buffer_alloc(64);
    if (!buf) return SG_ERR_NOMEM;

    unsigned int out_len = 0;
    if (EVP_DigestFinal_ex(static_cast<EVP_MD_CTX*>(digest_context), signal_buffer_data(buf), &out_len) != 1) {
        signal_buffer_free(buf);
        return SG_ERR_UNKNOWN;
    }

    // Re-init for reuse
    EVP_DigestInit_ex(static_cast<EVP_MD_CTX*>(digest_context), EVP_sha512(), nullptr);

    *output = buf;
    return 0;
}

static void sha512_digest_cleanup(void* digest_context, void* user_data) {
    (void)user_data;
    EVP_MD_CTX_free(static_cast<EVP_MD_CTX*>(digest_context));
}

// AES encrypt/decrypt for Signal Protocol
static int aes_encrypt(signal_buffer** output, int cipher_mode,
                       const uint8_t* key, size_t key_len,
                       const uint8_t* iv, size_t iv_len,
                       const uint8_t* plaintext, size_t plaintext_len,
                       void* user_data) {
    (void)user_data;
    (void)iv_len;

    const EVP_CIPHER* cipher = nullptr;
    if (key_len == 16) {
        cipher = (cipher_mode == SG_CIPHER_AES_CBC_PKCS5) ? EVP_aes_128_cbc() : EVP_aes_128_ctr();
    } else if (key_len == 24) {
        cipher = (cipher_mode == SG_CIPHER_AES_CBC_PKCS5) ? EVP_aes_192_cbc() : EVP_aes_192_ctr();
    } else if (key_len == 32) {
        cipher = (cipher_mode == SG_CIPHER_AES_CBC_PKCS5) ? EVP_aes_256_cbc() : EVP_aes_256_ctr();
    } else {
        return SG_ERR_UNKNOWN;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return SG_ERR_UNKNOWN;

    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }

    if (cipher_mode == SG_CIPHER_AES_CTR_NOPADDING) {
        EVP_CIPHER_CTX_set_padding(ctx, 0);
    }

    signal_buffer* buf = signal_buffer_alloc(plaintext_len + EVP_CIPHER_block_size(cipher));
    if (!buf) {
        EVP_CIPHER_CTX_free(ctx);
        return SG_ERR_NOMEM;
    }

    int out_len = 0, final_len = 0;
    if (EVP_EncryptUpdate(ctx, signal_buffer_data(buf), &out_len, plaintext, static_cast<int>(plaintext_len)) != 1) {
        signal_buffer_free(buf);
        EVP_CIPHER_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }

    if (EVP_EncryptFinal_ex(ctx, signal_buffer_data(buf) + out_len, &final_len) != 1) {
        signal_buffer_free(buf);
        EVP_CIPHER_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }

    signal_buffer* result = signal_buffer_create(signal_buffer_data(buf), out_len + final_len);
    signal_buffer_free(buf);
    EVP_CIPHER_CTX_free(ctx);
    if (!result) return SG_ERR_NOMEM;
    *output = result;
    return 0;
}

static int aes_decrypt(signal_buffer** output, int cipher_mode,
                       const uint8_t* key, size_t key_len,
                       const uint8_t* iv, size_t iv_len,
                       const uint8_t* ciphertext, size_t ciphertext_len,
                       void* user_data) {
    (void)user_data;
    (void)iv_len;

    const EVP_CIPHER* cipher = nullptr;
    if (key_len == 16) {
        cipher = (cipher_mode == SG_CIPHER_AES_CBC_PKCS5) ? EVP_aes_128_cbc() : EVP_aes_128_ctr();
    } else if (key_len == 24) {
        cipher = (cipher_mode == SG_CIPHER_AES_CBC_PKCS5) ? EVP_aes_192_cbc() : EVP_aes_192_ctr();
    } else if (key_len == 32) {
        cipher = (cipher_mode == SG_CIPHER_AES_CBC_PKCS5) ? EVP_aes_256_cbc() : EVP_aes_256_ctr();
    } else {
        return SG_ERR_UNKNOWN;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return SG_ERR_UNKNOWN;

    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }

    if (cipher_mode == SG_CIPHER_AES_CTR_NOPADDING) {
        EVP_CIPHER_CTX_set_padding(ctx, 0);
    }

    signal_buffer* buf = signal_buffer_alloc(ciphertext_len + EVP_CIPHER_block_size(cipher));
    if (!buf) {
        EVP_CIPHER_CTX_free(ctx);
        return SG_ERR_NOMEM;
    }

    int out_len = 0, final_len = 0;
    if (EVP_DecryptUpdate(ctx, signal_buffer_data(buf), &out_len, ciphertext, static_cast<int>(ciphertext_len)) != 1) {
        signal_buffer_free(buf);
        EVP_CIPHER_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }

    if (EVP_DecryptFinal_ex(ctx, signal_buffer_data(buf) + out_len, &final_len) != 1) {
        signal_buffer_free(buf);
        EVP_CIPHER_CTX_free(ctx);
        return SG_ERR_UNKNOWN;
    }

    signal_buffer* result = signal_buffer_create(signal_buffer_data(buf), out_len + final_len);
    signal_buffer_free(buf);
    EVP_CIPHER_CTX_free(ctx);
    if (!result) return SG_ERR_NOMEM;
    *output = result;
    return 0;
}

// ============================================================================
// Signal Protocol Store Callbacks
// ============================================================================

static CryptoEngine* g_engine = nullptr;

// Session store callbacks
static int session_store_load_session_func(signal_buffer** record, signal_buffer** user_record, const signal_protocol_address* address, void* user_data) {
    (void)user_data;
    (void)user_record;
    auto* engine = g_engine;
    if (!engine || !engine->getStore()) return 0;

    std::string addr = std::string(address->name, address->name_len) + ":" +
                       std::to_string(address->device_id);

    auto data = engine->getStore()->loadSession(addr);
    if (data.empty()) return 0;

    *record = signal_buffer_create(data.data(), data.size());
    if (!*record) return SG_ERR_NOMEM;
    return 1;
}

static int session_store_get_sub_device_sessions_func(signal_int_list** sessions, const char* name, size_t name_len, void* user_data) {
    (void)user_data;
    (void)name;
    (void)name_len;
    // For simplicity, return empty list - we use single device per user
    *sessions = signal_int_list_alloc();
    return 0;
}

static int session_store_store_session_func(const signal_protocol_address* address, uint8_t* record, size_t record_len, uint8_t* user_record, size_t user_record_len, void* user_data) {
    (void)user_data;
    (void)user_record;
    (void)user_record_len;
    auto* engine = g_engine;
    if (!engine || !engine->getStore()) return SG_ERR_UNKNOWN;

    std::string addr = std::string(address->name, address->name_len) + ":" +
                       std::to_string(address->device_id);

    std::vector<uint8_t> data(record, record + record_len);
    engine->getStore()->saveSession(addr, data);
    return 0;
}

static int session_store_contains_session_func(const signal_protocol_address* address, void* user_data) {
    (void)user_data;
    auto* engine = g_engine;
    if (!engine || !engine->getStore()) return 0;

    std::string addr = std::string(address->name, address->name_len) + ":" +
                       std::to_string(address->device_id);

    auto data = engine->getStore()->loadSession(addr);
    return !data.empty() ? 1 : 0;
}

static int session_store_delete_session_func(const signal_protocol_address* address, void* user_data) {
    (void)user_data;
    (void)address;
    // Not implemented - sessions are not deleted in our design
    return 0;
}

static int session_store_delete_all_sessions_func(const char* name, size_t name_len, void* user_data) {
    (void)user_data;
    (void)name;
    (void)name_len;
    return 0;
}

// Pre-key store callbacks
static int pre_key_store_load_pre_key_func(signal_buffer** record, uint32_t pre_key_id, void* user_data) {
    (void)user_data;
    auto* engine = g_engine;
    if (!engine || !engine->getStore()) return SG_ERR_UNKNOWN;
    
    auto data = engine->getStore()->loadPreKey(pre_key_id);
    if (data.empty()) return SG_ERR_INVALID_KEY_ID;
    
    *record = signal_buffer_alloc(data.size());
    if (!*record) return SG_ERR_NOMEM;
    memcpy(signal_buffer_data(*record), data.data(), data.size());
    return 0;
}

static int pre_key_store_store_pre_key_func(uint32_t pre_key_id, uint8_t* record, size_t record_len, void* user_data) {
    (void)user_data;
    auto* engine = g_engine;
    if (!engine || !engine->getStore()) return SG_ERR_UNKNOWN;
    
    std::vector<uint8_t> data(record, record + record_len);
    engine->getStore()->savePreKey(pre_key_id, data);
    return 0;
}

static int pre_key_store_contains_pre_key_func(uint32_t pre_key_id, void* user_data) {
    (void)user_data;
    auto* engine = g_engine;
    if (!engine || !engine->getStore()) return 0;
    
    auto data = engine->getStore()->loadPreKey(pre_key_id);
    return !data.empty() ? 1 : 0;
}

static int pre_key_store_remove_pre_key_func(uint32_t pre_key_id, void* user_data) {
    (void)user_data;
    auto* engine = g_engine;
    if (!engine || !engine->getStore()) return SG_ERR_UNKNOWN;
    
    engine->getStore()->removePreKey(pre_key_id);
    return 0;
}

// Signed pre-key store callbacks
static int signed_pre_key_store_load_signed_pre_key_func(signal_buffer** record, uint32_t signed_pre_key_id, void* user_data) {
    (void)user_data;
    auto* engine = g_engine;
    if (!engine || !engine->getStore()) return SG_ERR_UNKNOWN;
    
    auto data = engine->getStore()->loadSignedPreKey(signed_pre_key_id);
    if (data.empty()) return SG_ERR_INVALID_KEY_ID;
    
    *record = signal_buffer_alloc(data.size());
    if (!*record) return SG_ERR_NOMEM;
    memcpy(signal_buffer_data(*record), data.data(), data.size());
    return 0;
}

static int signed_pre_key_store_store_signed_pre_key_func(uint32_t signed_pre_key_id, uint8_t* record, size_t record_len, void* user_data) {
    (void)user_data;
    auto* engine = g_engine;
    if (!engine || !engine->getStore()) return SG_ERR_UNKNOWN;
    
    std::vector<uint8_t> data(record, record + record_len);
    engine->getStore()->saveSignedPreKey(signed_pre_key_id, data);
    return 0;
}

static int signed_pre_key_store_contains_signed_pre_key_func(uint32_t signed_pre_key_id, void* user_data) {
    (void)user_data;
    auto* engine = g_engine;
    if (!engine || !engine->getStore()) return 0;
    
    auto data = engine->getStore()->loadSignedPreKey(signed_pre_key_id);
    return !data.empty() ? 1 : 0;
}

static int signed_pre_key_store_remove_signed_pre_key_func(uint32_t signed_pre_key_id, void* user_data) {
    (void)user_data;
    (void)signed_pre_key_id;
    // Not implemented
    return 0;
}

// Identity key store callbacks
static int identity_key_store_get_identity_key_pair_func(signal_buffer** public_data, signal_buffer** private_data, void* user_data) {
    (void)user_data;
    LOGD("=================================================================");
    LOGD("identity_key_store_get_identity_key_pair: CALLED");
    LOGD("=================================================================");
    
    auto* engine = g_engine;
    if (!engine) {
        LOGE("identity_key_store_get_identity_key_pair: engine is null");
        return SG_ERR_UNKNOWN;
    }
    
    // Check if X25519 keys have been generated (not just if engine is "ready")
    // Keys are available after convertEd25519ToX25519() succeeds
    const auto& pub_key = engine->getSignalIdentityPub();
    const auto& priv_key = engine->getSignalIdentityPriv();
    
    // Check if keys are all zeros (not yet initialized)
    bool pub_all_zero = true, priv_all_zero = true;
    for (int i = 0; i < 32; i++) {
        if (pub_key[i] != 0) pub_all_zero = false;
        if (priv_key[i] != 0) priv_all_zero = false;
    }
    if (pub_all_zero || priv_all_zero) {
        LOGE("identity_key_store_get_identity_key_pair: X25519 keys not initialized");
        return SG_ERR_UNKNOWN;
    }
    
    // Return X25519 keys (converted from Ed25519)
    // Signal Protocol requires Curve25519/X25519 format
    *public_data = signal_buffer_alloc(32);
    *private_data = signal_buffer_alloc(32);
    if (!*public_data || !*private_data) {
        LOGE("identity_key_store_get_identity_key_pair: failed to allocate buffers");
        signal_buffer_free(*public_data);
        signal_buffer_free(*private_data);
        return SG_ERR_NOMEM;
    }
    
    LOGD("identity_key_store_get_identity_key_pair: pub=%02x%02x...%02x%02x priv=%02x%02x...%02x%02x",
         pub_key[0], pub_key[1], pub_key[30], pub_key[31],
         priv_key[0], priv_key[1], priv_key[30], priv_key[31]);
    
    memcpy(signal_buffer_data(*public_data), pub_key.data(), 32);
    memcpy(signal_buffer_data(*private_data), priv_key.data(), 32);
    
    LOGD("identity_key_store_get_identity_key_pair: success");
    return 0;
}

static int identity_key_store_get_local_registration_id_func(void* user_data, uint32_t* registration_id) {
    (void)user_data;
    LOGD("identity_key_store_get_local_registration_id: called, returning 1");
    // Return a fixed registration ID for simplicity
    *registration_id = 1;
    return 0;
}

static int identity_key_store_save_identity_func(const signal_protocol_address* address, uint8_t* key_data, size_t key_len, void* user_data) {
    (void)user_data;
    auto* engine = g_engine;
    if (!engine || !engine->getStore()) return SG_ERR_UNKNOWN;
    
    std::string user_id(address->name);
    std::vector<uint8_t> key(key_data, key_data + key_len);
    engine->getStore()->savePeerIdentity(user_id, key);
    return 0;
}

static int identity_key_store_is_trusted_identity_func(const signal_protocol_address* address, uint8_t* key_data, size_t key_len, void* user_data) {
    (void)user_data;
    auto* engine = g_engine;
    if (!engine || !engine->getStore()) return 1; // Trust on first use
    
    std::string user_id(address->name);
    auto existing = engine->getStore()->loadPeerIdentity(user_id);
    
    if (existing.empty()) return 1; // Trust on first use
    
    // Check if keys match
    if (existing.size() != key_len) return 0;
    return (memcmp(existing.data(), key_data, key_len) == 0) ? 1 : 0;
}

static void identity_key_store_destroy_func(void* user_data) {
    (void)user_data;
}

// Sender key store callbacks (for group sessions)
static int sender_key_store_store_sender_key_func(const signal_protocol_sender_key_name* sender_key_name,
                                                   uint8_t* record, size_t record_len,
                                                   uint8_t* user_record, size_t user_record_len,
                                                   void* user_data) {
    (void)user_record;
    (void)user_record_len;
    (void)user_data;
    
    auto* engine = g_engine;
    if (!engine || !engine->getStore()) {
        LOGE("sender_key_store_store: engine or store not available");
        return SG_ERR_UNKNOWN;
    }
    
    std::string group_id(sender_key_name->group_id, sender_key_name->group_id_len);
    std::string sender_id(sender_key_name->sender.name, sender_key_name->sender.name_len);
    std::string key_id = sender_id + ":" + group_id + ":" + std::to_string(sender_key_name->sender.device_id);
    
    LOGD("=================================================================");
    LOGD("SAVING sender key for %s (%zu bytes)", key_id.c_str(), record_len);
    std::vector<uint8_t> data(record, record + record_len);
    engine->getStore()->saveSenderKey(key_id, data);
    LOGD("Saved sender key for %s", key_id.c_str());
    LOGD("=================================================================");
    return 0;
}

static int sender_key_store_load_sender_key_func(signal_buffer** record, signal_buffer** user_record,
                                                  const signal_protocol_sender_key_name* sender_key_name,
                                                  void* user_data) {
    (void)user_record;
    (void)user_data;
    
    auto* engine = g_engine;
    if (!engine || !engine->getStore()) {
        LOGE("sender_key_store_load: engine or store not available");
        return SG_ERR_UNKNOWN;
    }
    
    std::string group_id(sender_key_name->group_id, sender_key_name->group_id_len);
    std::string sender_id(sender_key_name->sender.name, sender_key_name->sender.name_len);
    std::string key_id = sender_id + ":" + group_id + ":" + std::to_string(sender_key_name->sender.device_id);
    
    LOGD("----------------------------------------------------------------");
    LOGD("Loading sender key for %s", key_id.c_str());
    auto data = engine->getStore()->loadSenderKey(key_id);
    if (data.empty()) {
        // No record found - return 0 with NULL buffer (not found)
        LOGD("No sender key found for %s - returning NULL (not found)", key_id.c_str());
        *record = nullptr;
        LOGD("----------------------------------------------------------------");
        return 0;  // 0 = not found, but not an error
    }
    
    *record = signal_buffer_create(data.data(), data.size());
    LOGD("Loaded sender key for %s (%zu bytes)", key_id.c_str(), data.size());
    LOGD("----------------------------------------------------------------");
    return 1;  // 1 = found
}

// ============================================================================
// CryptoEngine Implementation
// ============================================================================

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
    if (g_engine == this) {
        g_engine = nullptr;
    }
}

bool CryptoEngine::init(IStore* store, const std::string& user_id, const std::string& passphrase) {
    LOGD("CryptoEngine::init - starting");
    
    store_ = store;
    user_id_ = user_id;
    g_engine = this;
    
    // Initialize Signal Protocol context
    int result = signal_context_create(&signal_ctx_, nullptr);
    if (result != 0) {
        LOGE("Failed to create signal context: %d", result);
        return false;
    }

    // Set crypto provider
    signal_crypto_provider crypto_provider = {};
    crypto_provider.random_func = random_bytes;
    crypto_provider.hmac_sha256_init_func = hmac_sha256_init;
    crypto_provider.hmac_sha256_update_func = hmac_sha256_update;
    crypto_provider.hmac_sha256_final_func = hmac_sha256_final;
    crypto_provider.hmac_sha256_cleanup_func = hmac_sha256_cleanup;
    crypto_provider.sha512_digest_init_func = sha512_digest_init;
    crypto_provider.sha512_digest_update_func = sha512_digest_update;
    crypto_provider.sha512_digest_final_func = sha512_digest_final;
    crypto_provider.sha512_digest_cleanup_func = sha512_digest_cleanup;
    crypto_provider.encrypt_func = aes_encrypt;
    crypto_provider.decrypt_func = aes_decrypt;
    crypto_provider.user_data = nullptr;

    result = signal_context_set_crypto_provider(signal_ctx_, &crypto_provider);
    if (result != 0) {
        LOGE("Failed to set crypto provider: %d", result);
        return false;
    }

    // Set log level
    signal_context_set_log_function(signal_ctx_, [](int level, const char* message, size_t len, void* user_data) {
        (void)user_data;
        (void)len;
        if (level >= SG_LOG_ERROR) {
            LOGE("Signal: %s", message);
        } else if (level >= SG_LOG_NOTICE) {
            LOGD("Signal: %s", message);
        }
    });
    
    // Create store context
    result = signal_protocol_store_context_create(&store_ctx_, signal_ctx_);
    if (result != 0) {
        LOGE("Failed to create store context: %d", result);
        return false;
    }
    
    // Set up session store (using member variable for persistence)
    session_store_ = {
        .load_session_func = session_store_load_session_func,
        .get_sub_device_sessions_func = session_store_get_sub_device_sessions_func,
        .store_session_func = session_store_store_session_func,
        .contains_session_func = session_store_contains_session_func,
        .delete_session_func = session_store_delete_session_func,
        .delete_all_sessions_func = session_store_delete_all_sessions_func,
        .destroy_func = nullptr,
        .user_data = nullptr
    };
    signal_protocol_store_context_set_session_store(store_ctx_, &session_store_);
    LOGD("Session store configured");
    
    // Set up pre-key store (using member variable for persistence)
    pre_key_store_ = {};
    pre_key_store_.load_pre_key = pre_key_store_load_pre_key_func;
    pre_key_store_.store_pre_key = pre_key_store_store_pre_key_func;
    pre_key_store_.contains_pre_key = pre_key_store_contains_pre_key_func;
    pre_key_store_.remove_pre_key = pre_key_store_remove_pre_key_func;
    pre_key_store_.destroy_func = nullptr;
    pre_key_store_.user_data = nullptr;
    signal_protocol_store_context_set_pre_key_store(store_ctx_, &pre_key_store_);
    LOGD("Pre-key store configured");
    
    // Set up signed pre-key store (using member variable for persistence)
    signed_pre_key_store_ = {};
    signed_pre_key_store_.load_signed_pre_key = signed_pre_key_store_load_signed_pre_key_func;
    signed_pre_key_store_.store_signed_pre_key = signed_pre_key_store_store_signed_pre_key_func;
    signed_pre_key_store_.contains_signed_pre_key = signed_pre_key_store_contains_signed_pre_key_func;
    signed_pre_key_store_.remove_signed_pre_key = signed_pre_key_store_remove_signed_pre_key_func;
    signed_pre_key_store_.destroy_func = nullptr;
    signed_pre_key_store_.user_data = nullptr;
    signal_protocol_store_context_set_signed_pre_key_store(store_ctx_, &signed_pre_key_store_);
    LOGD("Signed pre-key store configured");
    
    // Set up identity key store (using member variable for persistence)
    identity_key_store_ = {};
    identity_key_store_.get_identity_key_pair = identity_key_store_get_identity_key_pair_func;
    identity_key_store_.get_local_registration_id = identity_key_store_get_local_registration_id_func;
    identity_key_store_.save_identity = identity_key_store_save_identity_func;
    identity_key_store_.is_trusted_identity = identity_key_store_is_trusted_identity_func;
    identity_key_store_.destroy_func = identity_key_store_destroy_func;
    identity_key_store_.user_data = nullptr;
    LOGD("Setting up identity key store:");
    LOGD("  get_identity_key_pair = %p", (void*)identity_key_store_.get_identity_key_pair);
    LOGD("  get_local_registration_id = %p", (void*)identity_key_store_.get_local_registration_id);
    LOGD("  save_identity = %p", (void*)identity_key_store_.save_identity);
    LOGD("  is_trusted_identity = %p", (void*)identity_key_store_.is_trusted_identity);
    LOGD("  destroy_func = %p", (void*)identity_key_store_.destroy_func);
    result = signal_protocol_store_context_set_identity_key_store(store_ctx_, &identity_key_store_);
    LOGD("Identity key store configured, result=%d", result);
    if (result != 0) {
        LOGE("Failed to set identity key store: %d", result);
        return false;
    }
    
    // Set up sender key store (for group sessions, using member variable for persistence)
    sender_key_store_ = {};
    sender_key_store_.store_sender_key = sender_key_store_store_sender_key_func;
    sender_key_store_.load_sender_key = sender_key_store_load_sender_key_func;
    sender_key_store_.destroy_func = nullptr;
    sender_key_store_.user_data = nullptr;
    LOGD("Setting up sender key store:");
    LOGD("  store_sender_key = %p", (void*)sender_key_store_.store_sender_key);
    LOGD("  load_sender_key = %p", (void*)sender_key_store_.load_sender_key);
    result = signal_protocol_store_context_set_sender_key_store(store_ctx_, &sender_key_store_);
    LOGD("Sender key store configured, result=%d", result);
    if (result != 0) {
        LOGE("Failed to set sender key store: %d", result);
        return false;
    }
    
    // Load or generate identity
    if (!loadOrGenerateIdentity(passphrase)) {
        LOGE("Failed to load or generate identity");
        return false;
    }
    
    // Generate X25519 keys from Ed25519 for Signal Protocol
    // Signal Protocol uses Curve25519/X25519, not Ed25519
    // We use OpenSSL for the conversion
    if (!convertEd25519ToX25519()) {
        LOGE("Failed to convert Ed25519 identity keys to X25519");
        return false;
    }
    LOGD("Converted Ed25519 identity keys to X25519 format");
    LOGD("X25519 identity pub: %02x%02x...%02x%02x", 
         signal_identity_pub_[0], signal_identity_pub_[1],
         signal_identity_pub_[30], signal_identity_pub_[31]);
    
    // Test the identity key callback directly
    LOGD("Testing identity key callback...");
    signal_buffer *test_pub = nullptr, *test_priv = nullptr;
    int test_result = identity_key_store_get_identity_key_pair_func(&test_pub, &test_priv, nullptr);
    if (test_result != 0) {
        LOGE("Identity key callback test failed: %d", test_result);
        return false;
    }
    LOGD("Identity key callback test passed");
    signal_buffer_free(test_pub);
    signal_buffer_free(test_priv);

    // Create group session for sender key encryption
    group_session_ = std::make_unique<GroupSession>(store_ctx_, signal_ctx_);
    group_session_->set_local_identity(user_id_, 1);
    LOGD("Created GroupSession for sender key encryption, store_ctx=%p", store_ctx_);
    
    loaded_ = true;
    LOGD("CryptoEngine::init - success, ready=%d", ready());
    return true;
}

bool CryptoEngine::loadOrGenerateIdentity(const std::string& passphrase) {
    // Initialize key arrays to zero
    ed25519_key_.pub.fill(0);
    ed25519_key_.priv.fill(0);
    
    std::vector<uint8_t> pub_key, priv_key_encrypted, salt;
    
    if (store_->loadIdentity(user_id_, pub_key, priv_key_encrypted, salt)) {
        LOGD("Loaded existing identity");
        if (pub_key.size() != 32) {
            LOGE("Invalid public key size: %zu", pub_key.size());
            return false;
        }
        memcpy(ed25519_key_.pub.data(), pub_key.data(), 32);
        
        // Decrypt private key
        std::array<uint8_t, 64> priv_decrypted = {};
        if (!decryptIdentityPriv(priv_key_encrypted, salt, passphrase, priv_decrypted)) {
            LOGE("Failed to decrypt private key - cannot initialize crypto engine");
            return false;
        }
        // Copy only the 32-byte seed to the first 32 bytes of ed25519_key_.priv
        memcpy(ed25519_key_.priv.data(), priv_decrypted.data(), 32);
        LOGD("Decrypted private key successfully, pub=%02x%02x...%02x%02x",
             ed25519_key_.pub[0], ed25519_key_.pub[1],
             ed25519_key_.pub[30], ed25519_key_.pub[31]);
        return true;
    }
    
    LOGD("Generating new identity");
    return generateIdentity(passphrase);
}

// Convert Ed25519 keys to X25519 for Signal Protocol compatibility
// Ed25519 and X25519 use different curve representations but the same scalar field
bool CryptoEngine::convertEd25519ToX25519() {
    LOGD("Converting Ed25519 to X25519...");
    LOGD("Ed25519 pub: %02x%02x...%02x%02x",
         ed25519_key_.pub[0], ed25519_key_.pub[1],
         ed25519_key_.pub[30], ed25519_key_.pub[31]);
    LOGD("Ed25519 priv seed: %02x%02x...%02x%02x",
         ed25519_key_.priv[0], ed25519_key_.priv[1],
         ed25519_key_.priv[30], ed25519_key_.priv[31]);
    
    // The Ed25519 private key (32-byte seed) needs to be hashed and clamped for X25519
    // X25519 private key = first 32 bytes of SHA512(Ed25519 private key seed), with clamping
    
    // Hash the Ed25519 private key seed
    signal_buffer* hash = nullptr;
    // Use first 32 bytes of Ed25519 priv (the seed)
    sha512_digest_helper(&hash, ed25519_key_.priv.data(), 32);
    if (!hash) {
        LOGE("Failed to hash Ed25519 private key");
        return false;
    }
    
    // Copy first 32 bytes as X25519 private key
    memcpy(signal_identity_priv_.data(), signal_buffer_data(hash), 32);
    LOGD("X25519 priv (before clamping): %02x%02x...%02x%02x",
         signal_identity_priv_[0], signal_identity_priv_[1],
         signal_identity_priv_[30], signal_identity_priv_[31]);
    signal_buffer_free(hash);
    
    // Apply X25519 clamping: clear bit 0, 1, 2, set bit 254, clear bit 255
    signal_identity_priv_[0] &= 248;
    signal_identity_priv_[31] &= 127;
    signal_identity_priv_[31] |= 64;
    
    // Derive X25519 public key from private key using scalar multiplication
    // For now, we'll use OpenSSL's X25519 to generate the public key
    EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, 
                                                   signal_identity_priv_.data(), 32);
    if (!pkey) {
        LOGE("Failed to create X25519 private key");
        return false;
    }
    
    size_t pub_len = 32;
    if (EVP_PKEY_get_raw_public_key(pkey, signal_identity_pub_.data(), &pub_len) != 1) {
        LOGE("Failed to get X25519 public key");
        EVP_PKEY_free(pkey);
        return false;
    }
    
    EVP_PKEY_free(pkey);
    
    LOGD("Converted Ed25519 to X25519: pub=%02x%02x...%02x%02x priv=%02x%02x...%02x%02x",
         signal_identity_pub_[0], signal_identity_pub_[1], 
         signal_identity_pub_[30], signal_identity_pub_[31],
         signal_identity_priv_[0], signal_identity_priv_[1],
         signal_identity_priv_[30], signal_identity_priv_[31]);
    
    // Verify keys are not all zeros
    bool pub_all_zero = true, priv_all_zero = true;
    for (int i = 0; i < 32; i++) {
        if (signal_identity_pub_[i] != 0) pub_all_zero = false;
        if (signal_identity_priv_[i] != 0) priv_all_zero = false;
    }
    if (pub_all_zero || priv_all_zero) {
        LOGE("X25519 keys are all zeros!");
        return false;
    }
    
    return true;
}

bool CryptoEngine::generateIdentity(const std::string& passphrase) {
    // Initialize key arrays to zero
    ed25519_key_.pub.fill(0);
    ed25519_key_.priv.fill(0);
    
    // Generate Ed25519 identity key pair using OpenSSL
    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey) return false;
    
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return false;
    }
    
    if (EVP_PKEY_keygen_init(ctx) <= 0 || EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return false;
    }
    
    EVP_PKEY_CTX_free(ctx);
    
    // Get public key
    size_t pub_len = 32;
    if (EVP_PKEY_get_raw_public_key(pkey, ed25519_key_.pub.data(), &pub_len) != 1 || pub_len != 32) {
        EVP_PKEY_free(pkey);
        return false;
    }
    
    // Get private key (Ed25519 raw private key from OpenSSL is 32-byte seed)
    size_t priv_len = 32;
    std::array<uint8_t, 64> priv_key = {};
    if (EVP_PKEY_get_raw_private_key(pkey, priv_key.data(), &priv_len) != 1 || priv_len != 32) {
        EVP_PKEY_free(pkey);
        return false;
    }
    
    // Cache the seed in ed25519_key_.priv (first 32 bytes)
    memcpy(ed25519_key_.priv.data(), priv_key.data(), 32);

    EVP_PKEY_free(pkey);

    // Encrypt private key
    std::vector<uint8_t> salt;
    auto encrypted = encryptIdentityPriv(priv_key, passphrase, salt);
    
    // Save identity
    std::vector<uint8_t> pub_vec(ed25519_key_.pub.begin(), ed25519_key_.pub.end());
    store_->saveIdentity(user_id_, pub_vec, encrypted, salt);
    
    LOGD("Generated new identity, public key: %02x%02x...%02x%02x",
         ed25519_key_.pub[0], ed25519_key_.pub[1], ed25519_key_.pub[30], ed25519_key_.pub[31]);
    
    return true;
}

std::vector<uint8_t> CryptoEngine::encryptIdentityPriv(const std::array<uint8_t, 64>& priv_key,
                                                        const std::string& passphrase,
                                                        std::vector<uint8_t>& salt_out) {
    // Generate salt
    salt_out.resize(16);
    RAND_bytes(salt_out.data(), 16);
    
    // Derive key using PBKDF2 (in production, use Argon2id)
    std::array<uint8_t, 32> key;
    PKCS5_PBKDF2_HMAC(passphrase.c_str(), static_cast<int>(passphrase.length()),
                      salt_out.data(), static_cast<int>(salt_out.size()),
                      100000, EVP_sha256(),
                      32, key.data());
    
    // Encrypt with AES-256-GCM using OpenSSL
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    std::array<uint8_t, 12> iv;
    RAND_bytes(iv.data(), 12);
    
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    std::vector<uint8_t> ciphertext(priv_key.size() + 16); // +16 for GCM tag
    int len;
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, priv_key.data(), static_cast<int>(priv_key.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    int final_len;
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &final_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    // Get GCM tag
    std::array<uint8_t, 16> tag;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    EVP_CIPHER_CTX_free(ctx);
    
    // Format: [16 bytes IV] [ciphertext] [16 bytes tag]
    std::vector<uint8_t> result;
    result.reserve(iv.size() + ciphertext.size() + tag.size());
    result.insert(result.end(), iv.begin(), iv.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + len + final_len);
    result.insert(result.end(), tag.begin(), tag.end());
    
    return result;
}

bool CryptoEngine::decryptIdentityPriv(const std::vector<uint8_t>& ciphertext,
                                        const std::vector<uint8_t>& salt,
                                        const std::string& passphrase,
                                        std::array<uint8_t, 64>& priv_out) {
    if (ciphertext.size() < 12 + 16 + 16) return false; // IV + min ciphertext + tag
    
    // Derive key
    std::array<uint8_t, 32> key;
    PKCS5_PBKDF2_HMAC(passphrase.c_str(), static_cast<int>(passphrase.length()),
                      salt.data(), static_cast<int>(salt.size()),
                      100000, EVP_sha256(),
                      32, key.data());
    
    // Extract IV, ciphertext, and tag
    std::array<uint8_t, 12> iv;
    memcpy(iv.data(), ciphertext.data(), 12);
    
    size_t ct_len = ciphertext.size() - 12 - 16;
    std::vector<uint8_t> plaintext(ct_len);
    
    std::array<uint8_t, 16> tag;
    memcpy(tag.data(), ciphertext.data() + 12 + ct_len, 16);
    
    // Decrypt
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;
    
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    int len;
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data() + 12, static_cast<int>(ct_len)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // Set expected tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    int final_len;
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &final_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    EVP_CIPHER_CTX_free(ctx);
    
    if (plaintext.size() != 64) return false;
    memcpy(priv_out.data(), plaintext.data(), 64);
    return true;
}

std::vector<uint8_t> CryptoEngine::prepareKeyUpload(int num_opks) {
    LOGD("prepareKeyUpload: generating %d one-time pre-keys", num_opks);
    
    // We need an identity key pair for generating the signed pre-key
    // For now, generate a temporary one using curve_generate_key_pair
    // TODO: Use proper ratchet_identity_key_pair from Signal Protocol
    ratchet_identity_key_pair* identity_kp = nullptr;
    int result = signal_protocol_key_helper_generate_identity_key_pair(&identity_kp, signal_ctx_);
    if (result != 0 || !identity_kp) {
        LOGE("Failed to generate identity key pair for SPK signing");
        return {};
    }

    // Generate signed pre-key
    uint32_t spk_id = 1;
    session_signed_pre_key* spk = nullptr;
    uint64_t timestamp = static_cast<uint64_t>(time(nullptr)) * 1000;

    result = signal_protocol_key_helper_generate_signed_pre_key(&spk, identity_kp, spk_id, timestamp, signal_ctx_);
    SIGNAL_UNREF(identity_kp);

    if (result != 0 || !spk) {
        LOGE("Failed to create signed pre-key");
        return {};
    }

    spk_.id = spk_id;

    // Get public key from key pair
    ec_key_pair* spk_key_pair = session_signed_pre_key_get_key_pair(spk);
    if (spk_key_pair) {
        ec_public_key* pub = ec_key_pair_get_public(spk_key_pair);
        signal_buffer* pub_buf = nullptr;
        ec_public_key_serialize(&pub_buf, pub);
        if (pub_buf) {
            spk_.key_pair.pub.fill(0);
            size_t copy_len = std::min(size_t(32), signal_buffer_len(pub_buf));
            memcpy(spk_.key_pair.pub.data(), signal_buffer_data(pub_buf), copy_len);
            signal_buffer_free(pub_buf);
        }
    }

    // Get signature
    const uint8_t* sig = session_signed_pre_key_get_signature(spk);
    size_t sig_len = session_signed_pre_key_get_signature_len(spk);
    spk_.signature.assign(sig, sig + sig_len);

    // Store signed pre-key
    signal_buffer* buf = nullptr;
    session_signed_pre_key_serialize(&buf, spk);
    if (buf) {
        std::vector<uint8_t> data(signal_buffer_data(buf), signal_buffer_data(buf) + signal_buffer_len(buf));
        store_->saveSignedPreKey(spk_id, data);
        signal_buffer_free(buf);
    }

    SIGNAL_UNREF(spk);

    // Generate one-time pre-keys
    signal_protocol_key_helper_pre_key_list_node* key_list = nullptr;
    result = signal_protocol_key_helper_generate_pre_keys(&key_list, next_opk_id_, num_opks, signal_ctx_);
    
    // Collect OPK data for upload
    std::vector<std::pair<uint32_t, std::array<uint8_t, 32>>> opks;

    if (result == 0 && key_list) {
        signal_protocol_key_helper_pre_key_list_node* node = key_list;
        while (node) {
            session_pre_key* key = signal_protocol_key_helper_key_list_element(node);
            if (key) {
                uint32_t id = session_pre_key_get_id(key);

                // Save to local store
                signal_buffer* key_buf = nullptr;
                session_pre_key_serialize(&key_buf, key);
                if (key_buf) {
                    std::vector<uint8_t> data(signal_buffer_data(key_buf), signal_buffer_data(key_buf) + signal_buffer_len(key_buf));
                    store_->savePreKey(id, data);
                    signal_buffer_free(key_buf);
                }

                // Extract public key for upload
                ec_key_pair* kp = session_pre_key_get_key_pair(key);
                if (kp) {
                    ec_public_key* pub = ec_key_pair_get_public(kp);
                    signal_buffer* pub_buf = nullptr;
                    ec_public_key_serialize(&pub_buf, pub);
                    if (pub_buf) {
                        std::array<uint8_t, 32> pub_arr{};
                        size_t copy_len = std::min(size_t(32), signal_buffer_len(pub_buf));
                        memcpy(pub_arr.data(), signal_buffer_data(pub_buf), copy_len);
                        opks.push_back({id, pub_arr});
                        signal_buffer_free(pub_buf);
                    }
                }

                next_opk_id_ = id + 1;
            }
            node = signal_protocol_key_helper_key_list_next(node);
        }
        signal_protocol_key_helper_key_list_free(key_list);
    }
    
    // Build KeyUpload protobuf in simplified binary format
    // Format expected by parseKeyUploadBytes in Kotlin:
    // [spk_id: 4 bytes LE][spk_pub: 32 bytes][sig_len: 4 bytes LE][spk_sig: sig_len bytes]
    // [opk_count: 4 bytes LE][opk_id: 4 bytes LE][opk_pub: 32 bytes]...
    std::vector<uint8_t> upload;
    
    // SPK ID (little-endian)
    upload.push_back(spk_id & 0xFF);
    upload.push_back((spk_id >> 8) & 0xFF);
    upload.push_back((spk_id >> 16) & 0xFF);
    upload.push_back((spk_id >> 24) & 0xFF);
    
    // Signed pre-key public (32 bytes)
    upload.insert(upload.end(), spk_.key_pair.pub.begin(), spk_.key_pair.pub.end());
    
    // SPK signature length (little-endian) + signature
    uint32_t sig_len_u32 = static_cast<uint32_t>(spk_.signature.size());
    upload.push_back(sig_len_u32 & 0xFF);
    upload.push_back((sig_len_u32 >> 8) & 0xFF);
    upload.push_back((sig_len_u32 >> 16) & 0xFF);
    upload.push_back((sig_len_u32 >> 24) & 0xFF);
    upload.insert(upload.end(), spk_.signature.begin(), spk_.signature.end());
    
    // One-time pre-keys count (little-endian)
    uint32_t opk_count = static_cast<uint32_t>(opks.size());
    upload.push_back(opk_count & 0xFF);
    upload.push_back((opk_count >> 8) & 0xFF);
    upload.push_back((opk_count >> 16) & 0xFF);
    upload.push_back((opk_count >> 24) & 0xFF);
    
    // Serialize each OPK: [id: 4 bytes LE][pub: 32 bytes]
    for (const auto& [id, pub] : opks) {
        upload.push_back(id & 0xFF);
        upload.push_back((id >> 8) & 0xFF);
        upload.push_back((id >> 16) & 0xFF);
        upload.push_back((id >> 24) & 0xFF);
        upload.insert(upload.end(), pub.begin(), pub.end());
    }
    
    LOGD("prepareKeyUpload: generated upload data (%zu bytes) with %u OPKs", upload.size(), opk_count);
    return upload;
}

std::vector<uint8_t> CryptoEngine::signChallenge(const std::vector<uint8_t>& nonce) {
    // Sign nonce || user_id with Ed25519 private key
    // Server expects: signature = Ed25519_sign(private_key, nonce || user_id)
    
    // Verify we have a loaded private key
    bool has_private_key = false;
    for (size_t i = 0; i < ed25519_key_.priv.size(); i++) {
        if (ed25519_key_.priv[i] != 0) {
            has_private_key = true;
            break;
        }
    }
    if (!has_private_key) {
        LOGE("Cannot sign challenge: private key not loaded");
        return {};
    }
    
    // Construct data to sign: nonce || user_id
    std::vector<uint8_t> data_to_sign;
    data_to_sign.reserve(nonce.size() + user_id_.size());
    data_to_sign.insert(data_to_sign.end(), nonce.begin(), nonce.end());
    data_to_sign.insert(data_to_sign.end(), user_id_.begin(), user_id_.end());
    
    // Reconstruct EVP_PKEY from the cached 32-byte seed
    EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr,
                                                   ed25519_key_.priv.data(), 32);
    if (!pkey) {
        LOGE("Failed to create signing key from seed");
        return {};
    }

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        EVP_PKEY_free(pkey);
        return {};
    }

    std::vector<uint8_t> signature(64);
    size_t sig_len = 64;

    if (EVP_DigestSignInit(md_ctx, nullptr, nullptr, nullptr, pkey) != 1 ||
        EVP_DigestSign(md_ctx, signature.data(), &sig_len, data_to_sign.data(), data_to_sign.size()) != 1) {
        LOGE("Ed25519 signing failed");
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);
        return {};
    }

    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);

    signature.resize(sig_len);
    LOGD("Signed challenge with Ed25519 (%zu bytes) for user '%s'", sig_len, user_id_.c_str());
    return signature;
}

std::vector<uint8_t> CryptoEngine::identityPub() const {
    return std::vector<uint8_t>(ed25519_key_.pub.begin(), ed25519_key_.pub.end());
}

CryptoEngine::SpkInfo CryptoEngine::currentSpk() const {
    return SpkInfo{
        .pub = std::vector<uint8_t>(spk_.key_pair.pub.begin(), spk_.key_pair.pub.end()),
        .sig = spk_.signature,
        .id = spk_.id
    };
}

std::vector<uint8_t> CryptoEngine::encrypt(const std::string& recipient_id, 
                                            const std::string& plaintext) {
    if (!hasSession(recipient_id)) {
        LOGE("No session with %s", recipient_id.c_str());
        // Store as pending and return empty
        pending_plaintexts_[recipient_id] = plaintext;
        return {};
    }
    
    signal_protocol_address addr;
    addr.name = recipient_id.c_str();
    addr.name_len = recipient_id.length();
    addr.device_id = 1;
    
    session_cipher* cipher = nullptr;
    int ret = session_cipher_create(&cipher, store_ctx_, &addr, signal_ctx_);
    if (ret != 0 || !cipher) {
        LOGE("Failed to create session cipher: %d", ret);
        return {};
    }
    
    ciphertext_message* message = nullptr;
    ret = session_cipher_encrypt(cipher, reinterpret_cast<const uint8_t*>(plaintext.data()), 
                                 plaintext.length(), &message);
    if (ret != 0 || !message) {
        LOGE("Failed to encrypt: %d", ret);
        session_cipher_free(cipher);
        return {};
    }
    
    signal_buffer* serialized = ciphertext_message_get_serialized(message);
    std::vector<uint8_t> result_vec;
    if (serialized) {
        result_vec.assign(signal_buffer_data(serialized), 
                          signal_buffer_data(serialized) + signal_buffer_len(serialized));
    }
    
    SIGNAL_UNREF(message);
    session_cipher_free(cipher);
    
    return result_vec;
}

std::vector<uint8_t> CryptoEngine::encryptPending(const std::string& recipient_id) {
    auto it = pending_plaintexts_.find(recipient_id);
    if (it == pending_plaintexts_.end()) {
        return {};
    }
    
    auto result = encrypt(recipient_id, it->second);
    pending_plaintexts_.erase(it);
    return result;
}

DecryptResult CryptoEngine::decrypt(const std::string& sender_id,
                                     const std::string& recipient_id,
                                     const std::vector<uint8_t>& ciphertext,
                                     int ciphertext_type,
                                     const std::vector<uint8_t>& skdm) {
    (void)recipient_id;
    (void)skdm;
    
    DecryptResult result;
    result.sender_id = sender_id;
    
    signal_protocol_address addr;
    addr.name = sender_id.c_str();
    addr.name_len = sender_id.length();
    addr.device_id = 1;
    
    session_cipher* cipher = nullptr;
    int ret = session_cipher_create(&cipher, store_ctx_, &addr, signal_ctx_);
    if (ret != 0 || !cipher) {
        LOGE("Failed to create session cipher: %d", ret);
        return result;
    }
    
    signal_buffer* plaintext = nullptr;
    int decrypt_result = 0;
    
    if (ciphertext_type == 3) { // PRE_KEY_SIGNAL_MESSAGE
        pre_key_signal_message* msg = nullptr;
        decrypt_result = pre_key_signal_message_deserialize(&msg, ciphertext.data(), 
                                                            ciphertext.size(), signal_ctx_);
        if (decrypt_result == 0 && msg) {
            decrypt_result = session_cipher_decrypt_pre_key_signal_message(cipher, msg, nullptr, &plaintext);
            SIGNAL_UNREF(msg);
        } else {
            LOGE("Failed to deserialize pre-key signal message: %d", decrypt_result);
        }
    } else { // SIGNAL_MESSAGE
        signal_message* msg = nullptr;
        decrypt_result = signal_message_deserialize(&msg, ciphertext.data(), 
                                                    ciphertext.size(), signal_ctx_);
        if (decrypt_result == 0 && msg) {
            decrypt_result = session_cipher_decrypt_signal_message(cipher, msg, nullptr, &plaintext);
            SIGNAL_UNREF(msg);
        } else {
            LOGE("Failed to deserialize signal message: %d", decrypt_result);
        }
    }
    
    if (decrypt_result != 0 || !plaintext) {
        LOGE("Failed to decrypt: %d", decrypt_result);
        session_cipher_free(cipher);
        return result;
    }
    
    result.plaintext = std::string(reinterpret_cast<const char*>(signal_buffer_data(plaintext)),
                                    signal_buffer_len(plaintext));
    result.success = true;
    
    signal_buffer_free(plaintext);
    session_cipher_free(cipher);
    
    return result;
}

void CryptoEngine::onKeyBundle(const std::vector<uint8_t>& bundle_data,
                                const std::string& recipient_id) {
    LOGD("Processing key bundle for %s", recipient_id.c_str());
    
    // Parse bundle_data and create session
    // Format: [identity_pub: 32 bytes][spk_pub: 32 bytes][spk_sig: 64 bytes][spk_id: 4 bytes LE]
    if (bundle_data.size() < 32 + 32 + 64 + 4) {
        LOGE("Invalid key bundle size");
        return;
    }
    
    signal_protocol_address addr;
    addr.name = recipient_id.c_str();
    addr.name_len = recipient_id.length();
    addr.device_id = 1;
    
    session_builder* builder = nullptr;
    session_builder_create(&builder, store_ctx_, &addr, signal_ctx_);
    if (!builder) {
        LOGE("Failed to create session builder");
        return;
    }
    
    // TODO: Parse bundle and process using session_builder_process_pre_key_bundle
    
    session_builder_free(builder);
}

bool CryptoEngine::hasSession(const std::string& recipient_id) {
    signal_protocol_address addr;
    addr.name = recipient_id.c_str();
    addr.name_len = recipient_id.length();
    addr.device_id = 1;
    
    return session_store_contains_session_func(&addr, nullptr) == 1;
}

void CryptoEngine::initGroupSession(const std::string& channel_id, 
                                     const std::vector<std::string>& members) {
    (void)members; // Members not needed for Signal Protocol sender keys
    LOGD("initGroupSession for %s", channel_id.c_str());
    
    if (!group_session_) {
        LOGE("Group session not available");
        return;
    }
    
    try {
        // Create the sender key distribution message
        // This initializes our sender key state for this channel
        auto skdm = group_session_->create_session(channel_id);
        LOGD("Created group session for %s, SKDM size: %zu bytes", 
             channel_id.c_str(), skdm.size());
        
        // Store the SKDM for later distribution
        // In a full implementation, we'd send this to all channel members
        group_sessions_initialized_[channel_id] = true;
    } catch (const std::exception& e) {
        LOGE("Failed to create group session for %s: %s", channel_id.c_str(), e.what());
    }
}

CryptoEngine::GroupEncryptResult CryptoEngine::encryptGroup(const std::string& channel_id,
                                                               const std::string& plaintext) {
    GroupEncryptResult result;
    
    if (!group_session_) {
        LOGE("Group session not available");
        return result;
    }
    
    // Check if we need to create a session first
    auto it = group_sessions_initialized_.find(channel_id);
    bool is_first = (it == group_sessions_initialized_.end() || !it->second);
    
    if (is_first) {
        LOGD("Creating group session for %s on first encrypt", channel_id.c_str());
        try {
            result.skdm = group_session_->create_session(channel_id);
            group_sessions_initialized_[channel_id] = true;
            LOGD("Created group session for %s, SKDM size: %zu bytes",
                 channel_id.c_str(), result.skdm.size());
        } catch (const std::exception& e) {
            LOGE("Failed to create group session: %s", e.what());
            return result;
        }
    }
    
    try {
        std::vector<uint8_t> plain_vec(plaintext.begin(), plaintext.end());
        result.ciphertext = group_session_->encrypt(channel_id, plain_vec);
        LOGD("Encrypted group message for %s: %zu bytes", channel_id.c_str(), result.ciphertext.size());
        return result;
    } catch (const std::exception& e) {
        LOGE("Group encrypt failed for %s: %s", channel_id.c_str(), e.what());
        return result;
    }
}

std::vector<uint8_t> CryptoEngine::decryptGroup(const std::string& sender_id,
                                                 const std::string& channel_id,
                                                 const std::vector<uint8_t>& ciphertext,
                                                 const std::vector<uint8_t>& skdm) {
    if (!group_session_) {
        LOGE("Group session not available");
        return {};
    }
    
    // Process SKDM first if provided (first message from this sender)
    if (!skdm.empty()) {
        LOGD("Processing SKDM from %s for %s", sender_id.c_str(), channel_id.c_str());
        group_session_->process_sender_key_distribution(channel_id, sender_id, 1, skdm);
    }
    
    try {
        auto result = group_session_->decrypt(channel_id, sender_id, 1, ciphertext);
        LOGD("Decrypted group message from %s in %s: %zu bytes",
             sender_id.c_str(), channel_id.c_str(), result.size());
        return result;
    } catch (const std::exception& e) {
        LOGE("Group decrypt failed from %s in %s: %s",
             sender_id.c_str(), channel_id.c_str(), e.what());
        return {};
    }
}

void CryptoEngine::processSenderKeyDistribution(const std::string& sender_id,
                                                 const std::string& channel_id,
                                                 const std::vector<uint8_t>& skdm) {
    if (!group_session_) {
        LOGE("Group session not available");
        return;
    }
    
    LOGD("Processing SKDM from %s for %s", sender_id.c_str(), channel_id.c_str());
    group_session_->process_sender_key_distribution(channel_id, sender_id, 1, skdm);
}

std::string CryptoEngine::safetyNumber(const std::string& peer_id) {
    // Compute safety number from identity keys
    std::vector<uint8_t> our_key(ed25519_key_.pub.begin(), ed25519_key_.pub.end());
    auto their_key = store_->loadPeerIdentity(peer_id);
    
    if (their_key.empty()) {
        return "00000 00000 00000 00000 00000 00000 00000 00000 00000 00000 00000 00000";
    }
    
    // Sort keys to ensure deterministic order
    const uint8_t* first = our_key.data();
    const uint8_t* second = their_key.data();
    size_t first_len = our_key.size();
    size_t second_len = their_key.size();
    
    if (memcmp(our_key.data(), their_key.data(), std::min(our_key.size(), their_key.size())) > 0) {
        std::swap(first, second);
        std::swap(first_len, second_len);
    }
    
    // Compute hash
    std::vector<uint8_t> combined;
    combined.reserve(first_len + second_len);
    combined.insert(combined.end(), first, first + first_len);
    combined.insert(combined.end(), second, second + second_len);
    
    signal_buffer* hash = nullptr;
    sha512_digest_helper(&hash, combined.data(), combined.size());
    if (!hash) {
        return "00000 00000 00000 00000 00000 00000 00000 00000 00000 00000 00000 00000";
    }
    
    // Format as 60 digits in groups of 5
    std::string result;
    result.reserve(60 + 11); // 60 digits + 11 spaces
    
    const uint8_t* hash_data = signal_buffer_data(hash);
    for (int i = 0; i < 12; i++) {
        if (i > 0) result += ' ';
        // Use two bytes per 5-digit group
        uint16_t val = (hash_data[i * 2] << 8) | hash_data[i * 2 + 1];
        char buf[6];
        snprintf(buf, sizeof(buf), "%05d", val % 100000);
        result += buf;
    }
    
    signal_buffer_free(hash);
    return result;
}

std::string CryptoEngine::getPendingPlaintext(const std::string& recipient_id) {
    auto it = pending_plaintexts_.find(recipient_id);
    return (it != pending_plaintexts_.end()) ? it->second : "";
}

void CryptoEngine::clearPendingPlaintext(const std::string& recipient_id) {
    pending_plaintexts_.erase(recipient_id);
}

} // namespace grotto::crypto
