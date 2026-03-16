// Example: Bug report HTTP API endpoint
// This shows how to integrate grotto-server-api with grotto-server

#include <grotto/api/server.hpp>
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <mutex>
#include <chrono>

namespace api = grotto::api;
namespace asio = boost::asio;
using json = nlohmann::json;

struct BugReport {
    int id;
    std::string user_id;
    std::string description;
    int64_t timestamp;
};

class BugReportService {
public:
    int submit(const std::string& user_id, const std::string& description) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        BugReport report;
        report.id = next_id_++;
        report.user_id = user_id;
        report.description = description;
        report.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        reports_.push_back(report);
        save_to_file();
        
        return report.id;
    }
    
    std::vector<BugReport> list(int limit = 10) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<BugReport> result;
        int start = std::max(0, static_cast<int>(reports_.size()) - limit);
        
        for (int i = start; i < reports_.size(); ++i) {
            result.push_back(reports_[i]);
        }
        
        return result;
    }

private:
    void save_to_file() {
        // Persist to file...
    }
    
    mutable std::mutex mutex_;
    std::vector<BugReport> reports_;
    int next_id_ = 1;
};

int main() {
    asio::io_context io_context;
    
    // Service instance
    auto bug_service = std::make_shared<BugReportService>();
    
    // Server config
    api::ServerConfig config;
    config.port = 8080;
    config.api_keys = {grotto::api::ApiKeyManager::generate_key()};
    
    spdlog::info("API Key: {}", config.api_keys[0]);
    
    auto server = std::make_shared<api::Server>(io_context, config);
    
    // Health check (no auth required)
    server->get("/health", [](const api::Request& req) {
        return api::Response::ok({
            {"status", "ok"},
            {"service", "grotto-api"}
        });
    });
    
    // Submit bug report (no auth - public endpoint with rate limiting)
    server->post("/api/v1/bug-reports", [bug_service](const api::Request& req) {
        if (!req.has_json_body()) {
            return api::Response::bad_request("JSON body required");
        }
        
        auto body = req.json_body();
        
        if (!body.contains("description")) {
            return api::Response::bad_request("Missing 'description' field");
        }
        
        std::string description = body["description"];
        std::string user_id = body.value("user_id", "anonymous");
        
        if (description.empty() || description.size() < 10) {
            return api::Response::bad_request("Description too short (min 10 chars)");
        }
        
        int id = bug_service->submit(user_id, description);
        
        return api::Response::created({
            {"id", id},
            {"message", "Bug report submitted successfully"}
        });
    });
    
    // List bug reports (requires API key)
    server->get("/api/v1/bug-reports", [bug_service](const api::Request& req) {
        auto limit_param = req.query_param("limit");
        int limit = limit_param ? std::stoi(*limit_param) : 10;
        
        auto reports = bug_service->list(limit);
        
        json data = json::array();
        for (const auto& r : reports) {
            data.push_back({
                {"id", r.id},
                {"user_id", r.user_id},
                {"description", r.description},
                {"timestamp", r.timestamp}
            });
        }
        
        return api::Response::ok({{"reports", data}});
    });
    
    // Start server
    server->start();
    
    spdlog::info("Bug report API server running on http://localhost:8080");
    spdlog::info("Try: curl -X POST http://localhost:8080/api/v1/bug-reports \\");
    spdlog::info("     -H 'Content-Type: application/json' \\");
    spdlog::info("     -d '{\"description\": \"Test bug report\"}'");
    
    io_context.run();
    
    return 0;
}
