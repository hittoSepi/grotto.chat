#pragma once

#include <string>
#include <unordered_set>
#include <optional>

namespace grotto::api {

// Simple API key manager
class ApiKeyManager {
public:
    void add_key(const std::string& key);
    void remove_key(const std::string& key);
    bool is_valid(const std::string& key) const;
    
    // Generate a new random key
    static std::string generate_key();
    
    void clear();
    
private:
    std::unordered_set<std::string> keys_;
};

// Webhook secret manager
class WebhookManager {
public:
    void set_secret(const std::string& id, const std::string& secret);
    bool verify(const std::string& id, const std::string& signature, 
                const std::string& body) const;
    
private:
    std::unordered_map<std::string, std::string> secrets_;
};

} // namespace grotto::api
