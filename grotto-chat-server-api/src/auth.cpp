#include "grotto/api/auth.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <spdlog/spdlog.h>

#include <random>
#include <sstream>
#include <iomanip>

namespace grotto::api {

void ApiKeyManager::add_key(const std::string& key) {
    keys_.insert(key);
}

void ApiKeyManager::remove_key(const std::string& key) {
    keys_.erase(key);
}

bool ApiKeyManager::is_valid(const std::string& key) const {
    return keys_.find(key) != keys_.end();
}

std::string ApiKeyManager::generate_key() {
    // Generate 32 random bytes
    unsigned char random_bytes[32];
    if (RAND_bytes(random_bytes, sizeof(random_bytes)) != 1) {
        // Fallback to random_device if OpenSSL fails
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (auto& b : random_bytes) {
            b = static_cast<unsigned char>(dis(gen));
        }
    }
    
    // Convert to hex string
    std::ostringstream oss;
    oss << "grotto_";
    for (size_t i = 0; i < sizeof(random_bytes); ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(random_bytes[i]);
    }
    
    return oss.str();
}

void ApiKeyManager::clear() {
    keys_.clear();
}

void WebhookManager::set_secret(const std::string& id, const std::string& secret) {
    secrets_[id] = secret;
}

bool WebhookManager::verify(const std::string& id, const std::string& signature,
                           const std::string& body) const {
    auto it = secrets_.find(id);
    if (it == secrets_.end()) return false;
    
    // Simple HMAC-SHA256 verification
    const auto& secret = it->second;
    
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int result_len = 0;
    
    HMAC(EVP_sha256(), 
         reinterpret_cast<const unsigned char*>(secret.data()), secret.size(),
         reinterpret_cast<const unsigned char*>(body.data()), body.size(),
         result, &result_len);
    
    // Convert to hex string for comparison
    std::ostringstream computed;
    for (unsigned int i = 0; i < result_len; ++i) {
        computed << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(result[i]);
    }
    
    return computed.str() == signature;
}

} // namespace grotto::api
