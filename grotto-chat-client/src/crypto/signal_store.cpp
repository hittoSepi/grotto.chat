#include "crypto/signal_store.hpp"
#include <spdlog/spdlog.h>
#include <cstring>
#include <sodium.h>
#include <algorithm>
#include <string>
#include <vector>

// libsignal-protocol-c helpers
#include <signal_protocol_types.h>

namespace grotto::crypto {

namespace {

std::string short_hex(const uint8_t* data, size_t len, size_t max_bytes = 8) {
    if (!data || len == 0) return "(empty)";
    static constexpr char kHex[] = "0123456789abcdef";
    const size_t n = std::min(max_bytes, len);
    std::string out;
    out.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        const uint8_t b = data[i];
        out.push_back(kHex[b >> 4]);
        out.push_back(kHex[b & 0x0f]);
    }
    return out;
}

std::string private_fingerprint(const uint8_t* data, size_t len) {
    if (!data || len == 0) return "(empty)";
    std::array<uint8_t, 8> digest{};
    crypto_generichash(digest.data(), digest.size(), data, len, nullptr, 0);
    return short_hex(digest.data(), digest.size(), digest.size());
}

} // namespace

SignalStore::SignalStore(db::LocalStore& store, const std::string& local_user_id)
    : local_store_(store), local_user_id_(local_user_id) {}

SignalStore::~SignalStore() = default;

void SignalStore::set_identity_key(const std::array<uint8_t, 32>& pub_key,
                                   const std::array<uint8_t, 64>& priv_key) {
    identity_pub_ = pub_key;
    identity_priv_ = priv_key;
    identity_key_set_ = true;
}

void SignalStore::register_with_context(signal_protocol_store_context* ctx) {
    // ── Session store ─────────────────────────────────────────────────────
    session_store_.load_session_func            = session_load;
    session_store_.get_sub_device_sessions_func = session_get_sub_device_sessions;
    session_store_.store_session_func           = session_store_cb;
    session_store_.contains_session_func        = session_contains;
    session_store_.delete_session_func          = session_delete;
    session_store_.delete_all_sessions_func     = session_delete_all;
    session_store_.destroy_func                 = session_destroy;
    session_store_.user_data                    = this;
    signal_protocol_store_context_set_session_store(ctx, &session_store_);

    // ── Pre-key store ─────────────────────────────────────────────────────
    pre_key_store_.load_pre_key      = pre_key_load;
    pre_key_store_.store_pre_key     = pre_key_store_cb;
    pre_key_store_.contains_pre_key  = pre_key_contains;
    pre_key_store_.remove_pre_key    = pre_key_remove;
    pre_key_store_.destroy_func      = pre_key_destroy;
    pre_key_store_.user_data         = this;
    signal_protocol_store_context_set_pre_key_store(ctx, &pre_key_store_);

    // ── Signed pre-key store ──────────────────────────────────────────────
    spk_store_.load_signed_pre_key     = spk_load;
    spk_store_.store_signed_pre_key    = spk_store_cb;
    spk_store_.contains_signed_pre_key = spk_contains;
    spk_store_.remove_signed_pre_key   = spk_remove;
    spk_store_.destroy_func            = spk_destroy;
    spk_store_.user_data               = this;
    signal_protocol_store_context_set_signed_pre_key_store(ctx, &spk_store_);

    // ── Identity key store ────────────────────────────────────────────────
    id_store_.get_identity_key_pair         = id_get_key_pair;
    id_store_.get_local_registration_id     = id_get_local_registration;
    id_store_.save_identity                 = id_save_identity;
    id_store_.is_trusted_identity           = id_is_trusted;
    id_store_.destroy_func                  = id_destroy;
    id_store_.user_data                     = this;
    signal_protocol_store_context_set_identity_key_store(ctx, &id_store_);

    // ── Sender key store ──────────────────────────────────────────────────
    sk_store_.store_sender_key = sk_store_cb;
    sk_store_.load_sender_key  = sk_load;
    sk_store_.destroy_func     = sk_destroy;
    sk_store_.user_data        = this;
    signal_protocol_store_context_set_sender_key_store(ctx, &sk_store_);
}

// ── Session store callbacks ───────────────────────────────────────────────────

int SignalStore::session_load(signal_buffer** record, signal_buffer** /*user_record*/,
                               const signal_protocol_address* addr, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    std::string name(addr->name, addr->name_len);
    auto data = self->local_store_.load_session(name, addr->device_id);
    if (!data) return 0;
    *record = signal_buffer_create(data->data(), data->size());
    return 1;
}

int SignalStore::session_get_sub_device_sessions(signal_int_list** sessions,
    const char* name, size_t name_len, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    auto ids = self->local_store_.get_sub_device_sessions(std::string(name, name_len));
    *sessions = signal_int_list_alloc();
    if (!*sessions) return SG_ERR_NOMEM;
    for (int id : ids) signal_int_list_push_back(*sessions, id);
    return SG_SUCCESS;
}

int SignalStore::session_store_cb(const signal_protocol_address* addr,
    uint8_t* record, size_t record_len,
    uint8_t* /*user_record*/, size_t /*user_record_len*/, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    std::string name(addr->name, addr->name_len);
    std::vector<uint8_t> data(record, record + record_len);
    self->local_store_.save_session(name, addr->device_id, data);
    return SG_SUCCESS;
}

int SignalStore::session_contains(const signal_protocol_address* addr, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    std::string name(addr->name, addr->name_len);
    return self->local_store_.contains_session(name, addr->device_id) ? 1 : 0;
}

int SignalStore::session_delete(const signal_protocol_address* addr, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    std::string name(addr->name, addr->name_len);
    self->local_store_.delete_session(name, addr->device_id);
    return SG_SUCCESS;
}

int SignalStore::session_delete_all(const char* name, size_t name_len, void* ud) {
    // Not commonly needed; iterate sub-device sessions
    auto* self = static_cast<SignalStore*>(ud);
    auto ids = self->local_store_.get_sub_device_sessions(std::string(name, name_len));
    for (int id : ids) self->local_store_.delete_session(std::string(name, name_len), id);
    return SG_SUCCESS;
}

void SignalStore::session_destroy(void*) {}

// ── Pre-key callbacks ─────────────────────────────────────────────────────────

int SignalStore::pre_key_load(signal_buffer** record, uint32_t pre_key_id, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    auto data = self->local_store_.load_pre_key(pre_key_id);
    if (!data) return SG_ERR_INVALID_KEY_ID;
    *record = signal_buffer_create(data->data(), data->size());
    return SG_SUCCESS;
}

int SignalStore::pre_key_store_cb(uint32_t id, uint8_t* record, size_t len, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    self->local_store_.store_pre_key(id, std::vector<uint8_t>(record, record + len));
    return SG_SUCCESS;
}

int SignalStore::pre_key_remove(uint32_t id, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    self->local_store_.remove_pre_key(id);
    return SG_SUCCESS;
}

int SignalStore::pre_key_contains(uint32_t id, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    return self->local_store_.contains_pre_key(id) ? 1 : 0;
}

void SignalStore::pre_key_destroy(void*) {}

// ── Signed pre-key callbacks ──────────────────────────────────────────────────

int SignalStore::spk_load(signal_buffer** record, uint32_t id, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    auto data = self->local_store_.load_signed_pre_key(id);
    if (!data) return SG_ERR_INVALID_KEY_ID;
    *record = signal_buffer_create(data->data(), data->size());
    return SG_SUCCESS;
}

int SignalStore::spk_store_cb(uint32_t id, uint8_t* record, size_t len, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    self->local_store_.store_signed_pre_key(id, std::vector<uint8_t>(record, record + len));
    return SG_SUCCESS;
}

int SignalStore::spk_remove(uint32_t id, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    self->local_store_.remove_signed_pre_key(id);
    return SG_SUCCESS;
}

int SignalStore::spk_contains(uint32_t id, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    return self->local_store_.contains_signed_pre_key(id) ? 1 : 0;
}

void SignalStore::spk_destroy(void*) {}

// ── Identity key callbacks ────────────────────────────────────────────────────

int SignalStore::id_get_key_pair(signal_buffer** public_data,
                                  signal_buffer** private_data, void* ud) {
    // libsignal calls this to get our X3DH identity key pair.
    // Signal Protocol uses X25519 (Curve25519) keys.
    // libsignal's curve_decode_point expects a 33-byte key with 0x05 prefix.
    auto* self = static_cast<SignalStore*>(ud);
    
    // Use cached identity key if set (needed for group sessions)
    if (self->identity_key_set_) {
        // Convert Ed25519 keys to X25519 format for Signal Protocol
        std::array<uint8_t, 32> x25519_pub;
        std::array<uint8_t, 32> x25519_priv;
        std::array<uint8_t, 32> derived_pub_from_priv{};
        
        // Convert public key: Ed25519 -> X25519
        if (crypto_sign_ed25519_pk_to_curve25519(x25519_pub.data(), 
                                                  self->identity_pub_.data()) != 0) {
            return SG_ERR_UNKNOWN;
        }
        
        // Convert private key: Ed25519 -> X25519
        if (crypto_sign_ed25519_sk_to_curve25519(x25519_priv.data(),
                                                  self->identity_priv_.data()) != 0) {
            return SG_ERR_UNKNOWN;
        }

        if (crypto_scalarmult_base(derived_pub_from_priv.data(), x25519_priv.data()) != 0) {
            return SG_ERR_UNKNOWN;
        }
        
        // Prepend 0x05 (DJB_TYPE) for libsignal compatibility
        std::array<uint8_t, 33> pub_with_prefix{};
        pub_with_prefix[0] = 0x05;
        std::memcpy(pub_with_prefix.data() + 1, x25519_pub.data(), 32);

        const bool pair_matches = std::memcmp(
            derived_pub_from_priv.data(), x25519_pub.data(), x25519_pub.size()) == 0;

        spdlog::debug(
            "Identity keypair for libsignal (cached) user='{}': x25519_pub={} derived_pub={} pair_matches={} x25519_priv_fp={} ed25519_pub={}",
            self->local_user_id_,
            short_hex(x25519_pub.data(), x25519_pub.size()),
            short_hex(derived_pub_from_priv.data(), derived_pub_from_priv.size()),
            pair_matches,
            private_fingerprint(x25519_priv.data(), x25519_priv.size()),
            short_hex(self->identity_pub_.data(), self->identity_pub_.size()));
        
        *public_data  = signal_buffer_create(pub_with_prefix.data(), pub_with_prefix.size());
        *private_data = signal_buffer_create(x25519_priv.data(), x25519_priv.size());
        return (*public_data && *private_data) ? SG_SUCCESS : SG_ERR_NOMEM;
    }
    
    // Fallback: load from store (private key won't be available)
    spdlog::warn(
        "Identity keypair fallback hit for user='{}'; cached private key missing, returning public key only",
        self->local_user_id_);
    auto row = self->local_store_.load_identity(self->local_user_id_);
    if (!row) return SG_ERR_UNKNOWN;

    // Try to convert the public key from Ed25519 to X25519
    std::array<uint8_t, 33> pub_with_prefix{};
    pub_with_prefix[0] = 0x05;
    if (row->identity_pub.size() == 32 &&
        crypto_sign_ed25519_pk_to_curve25519(pub_with_prefix.data() + 1,
                                              row->identity_pub.data()) == 0) {
        *public_data = signal_buffer_create(pub_with_prefix.data(), pub_with_prefix.size());
        spdlog::debug(
            "Identity keypair fallback public user='{}': x25519_pub={} ed25519_pub={}",
            self->local_user_id_,
            short_hex(pub_with_prefix.data() + 1, 32),
            short_hex(row->identity_pub.data(), row->identity_pub.size()));
    } else {
        // Fallback: use as-is with prefix (may not work properly)
        std::vector<uint8_t> raw_with_prefix;
        raw_with_prefix.reserve(row->identity_pub.size() + 1);
        raw_with_prefix.push_back(0x05);
        raw_with_prefix.insert(raw_with_prefix.end(), row->identity_pub.begin(), row->identity_pub.end());
        *public_data = signal_buffer_create(raw_with_prefix.data(), raw_with_prefix.size());
        spdlog::warn(
            "Identity keypair fallback public user='{}' used raw key bytes len={}",
            self->local_user_id_,
            row->identity_pub.size());
    }
    
    // Private key not available - group sessions will fail
    *private_data = signal_buffer_create(nullptr, 0);
    return *public_data ? SG_SUCCESS : SG_ERR_NOMEM;
}

int SignalStore::id_get_local_registration(void* /*ud*/, uint32_t* registration_id) {
    // Use a fixed device id (single-device client)
    *registration_id = 1;
    return SG_SUCCESS;
}

int SignalStore::id_save_identity(const signal_protocol_address* addr,
                                   uint8_t* key_data, size_t key_len, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    std::string name(addr->name, addr->name_len);
    // Signal Protocol provides X25519 keys, but we store Ed25519 format
    // We store as-is for now; conversion happens in id_is_trusted
    self->local_store_.save_peer_identity(name,
        std::vector<uint8_t>(key_data, key_data + key_len));
    return SG_SUCCESS;
}

int SignalStore::id_is_trusted(const signal_protocol_address* addr,
                                uint8_t* key_data, size_t key_len, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    std::string name(addr->name, addr->name_len);
    auto stored = self->local_store_.load_peer_identity(name);
    if (!stored) {
        // First time seeing this identity: trust and save (TOFU)
        self->local_store_.save_peer_identity(name,
            std::vector<uint8_t>(key_data, key_data + key_len));
        return 1;
    }
    // libsignal always passes X25519 (Curve25519) keys here; stored keys are
    // also X25519 (saved as-is by id_save_identity and the TOFU path above).
    // Direct byte comparison is correct — no conversion needed.
    if (stored->size() != key_len) {
        spdlog::warn("Identity key size mismatch for {}: stored={} incoming={}", name, stored->size(), key_len);
        return 0;
    }
    return (std::memcmp(stored->data(), key_data, key_len) == 0) ? 1 : 0;
}

void SignalStore::id_destroy(void*) {}

// ── Sender key callbacks ──────────────────────────────────────────────────────

int SignalStore::sk_store_cb(const signal_protocol_sender_key_name* name,
                              uint8_t* record, size_t len,
                              uint8_t* /*user_record*/, size_t /*user_record_len*/, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    std::string group_id(name->group_id, name->group_id_len);
    std::string sender_id(name->sender.name, name->sender.name_len);
    self->local_store_.store_sender_key(group_id, sender_id, name->sender.device_id,
        std::vector<uint8_t>(record, record + len));
    return SG_SUCCESS;
}

int SignalStore::sk_load(signal_buffer** record, signal_buffer** /*user_record*/,
                          const signal_protocol_sender_key_name* name, void* ud) {
    auto* self = static_cast<SignalStore*>(ud);
    std::string group_id(name->group_id, name->group_id_len);
    std::string sender_id(name->sender.name, name->sender.name_len);
    auto data = self->local_store_.load_sender_key(group_id, sender_id, name->sender.device_id);
    if (!data) {
        // No record found - return 0 with NULL buffer (not found)
        *record = nullptr;
        return 0;
    }
    *record = signal_buffer_create(data->data(), data->size());
    return 1;  // 1 = found
}

void SignalStore::sk_destroy(void*) {}

} // namespace grotto::crypto
