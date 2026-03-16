#pragma once

#include <signal_protocol.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <array>

// Forward declaration for group session
namespace grotto::crypto {
    class GroupSession;
}

namespace grotto::crypto {

// Result structure for decryption
struct DecryptResult {
    std::string plaintext;
    std::string sender_id;
    bool success = false;
};

// Identity key pair structures
struct Ed25519KeyPair {
    std::array<uint8_t, 32> pub;
    std::array<uint8_t, 64> priv;
};

struct X25519KeyPair {
    std::array<uint8_t, 32> pub;
    std::array<uint8_t, 32> priv;
};

struct SignedPreKey {
    uint32_t id;
    X25519KeyPair key_pair;
    std::vector<uint8_t> signature;
};

// Group session state for Sender Keys
struct GroupSessionState {
    std::vector<uint8_t> chain_key;
    std::vector<uint8_t> signature_key_pub;
    std::vector<uint8_t> signature_key_priv;
    uint32_t iteration = 0;
    bool initialized = false;
};

// Simple storage interface for Android (backed by Java-side Room/SQLite)
class IStore {
public:
    virtual ~IStore() = default;
    
    // Identity - returns encrypted private key
    virtual void saveIdentity(const std::string& user_id, 
                              const std::vector<uint8_t>& pub_key,
                              const std::vector<uint8_t>& priv_key_encrypted,
                              const std::vector<uint8_t>& salt) = 0;
    virtual bool loadIdentity(const std::string& user_id,
                              std::vector<uint8_t>& pub_key_out,
                              std::vector<uint8_t>& priv_key_encrypted_out,
                              std::vector<uint8_t>& salt_out) = 0;
    
    // Sessions
    virtual void saveSession(const std::string& address, const std::vector<uint8_t>& record) = 0;
    virtual std::vector<uint8_t> loadSession(const std::string& address) = 0;
    
    // Pre-keys
    virtual void savePreKey(uint32_t id, const std::vector<uint8_t>& record) = 0;
    virtual std::vector<uint8_t> loadPreKey(uint32_t id) = 0;
    virtual void removePreKey(uint32_t id) = 0;
    
    // Signed pre-keys
    virtual void saveSignedPreKey(uint32_t id, const std::vector<uint8_t>& record) = 0;
    virtual std::vector<uint8_t> loadSignedPreKey(uint32_t id) = 0;
    
    // Peer identities
    virtual void savePeerIdentity(const std::string& user_id, const std::vector<uint8_t>& pub_key) = 0;
    virtual std::vector<uint8_t> loadPeerIdentity(const std::string& user_id) = 0;
    
    // Sender keys (group sessions)
    virtual void saveSenderKey(const std::string& sender_key_id, const std::vector<uint8_t>& record) = 0;
    virtual std::vector<uint8_t> loadSenderKey(const std::string& sender_key_id) = 0;
};

class CryptoEngine {
public:
    CryptoEngine();
    ~CryptoEngine();

    // Initialize with passphrase for identity encryption
    bool init(IStore* store, const std::string& user_id, const std::string& passphrase);
    
    bool ready() const { return loaded_; }
    IStore* getStore() const { return store_; }
    const std::string& getUserId() const { return user_id_; }

    // ── Registration ─────────────────────────────────────────────────────
    // Build KeyUpload protobuf bytes: SPK + OPKs
    std::vector<uint8_t> prepareKeyUpload(int num_opks = 100);

    // ── Auth ─────────────────────────────────────────────────────────────
    std::vector<uint8_t> signChallenge(const std::vector<uint8_t>& nonce);
    std::vector<uint8_t> identityPub() const;
    
    struct SpkInfo {
        std::vector<uint8_t> pub;
        std::vector<uint8_t> sig;
        uint32_t id;
    };
    SpkInfo currentSpk() const;

    // ── Encryption ────────────────────────────────────────────────────────
    // Returns serialized ChatEnvelope protobuf bytes
    std::vector<uint8_t> encrypt(const std::string& recipient_id, 
                                  const std::string& plaintext);
    
    // Encrypt with pending session (queue for after key bundle arrives)
    std::vector<uint8_t> encryptPending(const std::string& recipient_id);

    // ── Decryption ────────────────────────────────────────────────────────
    // ciphertext_type: 2=SIGNAL_MESSAGE, 3=PRE_KEY_SIGNAL_MESSAGE, 4=SENDER_KEY_MESSAGE
    DecryptResult decrypt(const std::string& sender_id,
                          const std::string& recipient_id,
                          const std::vector<uint8_t>& ciphertext,
                          int ciphertext_type,
                          const std::vector<uint8_t>& skdm = {});

    // ── Key bundle handling ────────────────────────────────────────────────
    // Process incoming KeyBundle and establish X3DH session
    void onKeyBundle(const std::vector<uint8_t>& bundle_data,
                     const std::string& recipient_id);

    // ── Group encryption (Sender Keys) ────────────────────────────────────
    struct GroupEncryptResult {
        std::vector<uint8_t> ciphertext;
        std::vector<uint8_t> skdm;  // SenderKeyDistributionMessage (empty if not first)
    };
    
    void initGroupSession(const std::string& channel_id, 
                          const std::vector<std::string>& members);
    GroupEncryptResult encryptGroup(const std::string& channel_id,
                                     const std::string& plaintext);
    std::vector<uint8_t> decryptGroup(const std::string& sender_id,
                                       const std::string& channel_id,
                                       const std::vector<uint8_t>& ciphertext,
                                       const std::vector<uint8_t>& skdm = {});
    
    // Process incoming SenderKeyDistributionMessage
    void processSenderKeyDistribution(const std::string& sender_id,
                                       const std::string& channel_id,
                                       const std::vector<uint8_t>& skdm);

    // ── Safety number ─────────────────────────────────────────────────────
    std::string safetyNumber(const std::string& peer_id);
    
    // Check if we have a session with this recipient
    bool hasSession(const std::string& recipient_id);
    
    // Get pending message for recipient (if waiting for key bundle)
    std::string getPendingPlaintext(const std::string& recipient_id);
    void clearPendingPlaintext(const std::string& recipient_id);
    
    // Get Signal Protocol identity keys (X25519 format) - public for store callbacks
    const std::array<uint8_t, 32>& getSignalIdentityPub() const { return signal_identity_pub_; }
    const std::array<uint8_t, 32>& getSignalIdentityPriv() const { return signal_identity_priv_; }

private:
    signal_context* signal_ctx_ = nullptr;
    signal_protocol_store_context* store_ctx_ = nullptr;
    
    IStore* store_ = nullptr;
    std::string user_id_;
    
    Ed25519KeyPair ed25519_key_;
    X25519KeyPair x25519_identity_;  // Separate X25519 identity for Signal
    SignedPreKey spk_;
    uint32_t next_opk_id_ = 1;
    bool loaded_ = false;
    
    // Pending messages waiting for key bundles
    std::unordered_map<std::string, std::string> pending_plaintexts_;
    
    // Signal Protocol group session for sending messages
    std::unique_ptr<GroupSession> group_session_;
    
    // Track which channels we've initialized sessions for
    std::unordered_map<std::string, bool> group_sessions_initialized_;
    
    void setupSignalCrypto(signal_context* ctx);
    bool loadOrGenerateIdentity(const std::string& passphrase);
    bool generateIdentity(const std::string& passphrase);
    
    // Identity key encryption with Argon2id + XChaCha20-Poly1305
    std::vector<uint8_t> encryptIdentityPriv(const std::array<uint8_t, 64>& priv_key,
                                              const std::string& passphrase,
                                              std::vector<uint8_t>& salt_out);
    bool decryptIdentityPriv(const std::vector<uint8_t>& ciphertext,
                             const std::vector<uint8_t>& salt,
                             const std::string& passphrase,
                             std::array<uint8_t, 64>& priv_out);
    
    // Convert Ed25519 identity keys to X25519 for Signal Protocol
    bool convertEd25519ToX25519();
    
    // Store callbacks - static functions that delegate to instance methods
    static void setupStores(signal_protocol_store_context* store_ctx, CryptoEngine* engine);
    
    // Helper for group session keys
    std::string makeGroupSessionKey(const std::string& sender_id, const std::string& channel_id) {
        return sender_id + ":" + channel_id;
    }
    
    // Identity key for Signal Protocol (X25519 format)
    std::array<uint8_t, 32> signal_identity_pub_{};
    std::array<uint8_t, 32> signal_identity_priv_{};
    
    // Signal Protocol store structures (must persist for callbacks)
    signal_protocol_session_store session_store_{};
    signal_protocol_pre_key_store pre_key_store_{};
    signal_protocol_signed_pre_key_store signed_pre_key_store_{};
    signal_protocol_identity_key_store identity_key_store_{};
    signal_protocol_sender_key_store sender_key_store_{};
};

} // namespace grotto::crypto
