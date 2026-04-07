#include "security/virus_scanner.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <sys/un.h>
#endif

namespace grotto::security {

namespace {

#ifdef _WIN32
constexpr VirusScanner::SocketHandle kInvalidSocket = static_cast<VirusScanner::SocketHandle>(INVALID_SOCKET);
#else
constexpr VirusScanner::SocketHandle kInvalidSocket = -1;
#endif

bool valid_socket(VirusScanner::SocketHandle sock) {
#ifdef _WIN32
    return sock != static_cast<VirusScanner::SocketHandle>(INVALID_SOCKET);
#else
    return sock >= 0;
#endif
}

std::string trim_response(std::string response) {
    while (!response.empty() &&
           (response.back() == '\0' || response.back() == '\n' || response.back() == '\r')) {
        response.pop_back();
    }
    return response;
}

bool send_all(VirusScanner::SocketHandle sock, const uint8_t* data, size_t size) {
    size_t sent = 0;
    while (sent < size) {
#ifdef _WIN32
        const int rc = ::send(
            static_cast<SOCKET>(sock),
            reinterpret_cast<const char*>(data + sent),
            static_cast<int>(size - sent),
            0);
#else
        const auto rc = ::send(sock, data + sent, size - sent, 0);
#endif
        if (rc <= 0) {
            return false;
        }
        sent += static_cast<size_t>(rc);
    }
    return true;
}

std::optional<std::string> receive_response(VirusScanner::SocketHandle sock) {
    std::string response;
    std::array<char, 256> buffer{};

    while (true) {
#ifdef _WIN32
        const int rc = ::recv(static_cast<SOCKET>(sock), buffer.data(), static_cast<int>(buffer.size()), 0);
#else
        const auto rc = ::recv(sock, buffer.data(), buffer.size(), 0);
#endif
        if (rc <= 0) {
            break;
        }
        response.append(buffer.data(), buffer.data() + rc);
        if (response.find('\0') != std::string::npos || response.find('\n') != std::string::npos) {
            break;
        }
    }

    response = trim_response(std::move(response));
    if (response.empty()) {
        return std::nullopt;
    }
    return response;
}

#ifdef _WIN32
void ensure_winsock_initialized() {
    static const bool initialized = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    (void)initialized;
}
#endif

} // namespace

VirusScanner::VirusScanner(const std::string& socket_path)
    : type_(ConnectionType::UnixSocket)
    , socket_path_(socket_path) {}

VirusScanner::VirusScanner(const std::string& host, uint16_t port)
    : type_(ConnectionType::Tcp)
    , host_(host)
    , port_(port) {}

bool VirusScanner::is_available() {
    return ping();
}

bool VirusScanner::ping() {
    const auto response = send_command("zPING");
    if (!response) {
        return false;
    }
    return trim_response(*response) == "PONG";
}

std::optional<std::string> VirusScanner::get_version() {
    return send_command("zVERSION");
}

VirusScanner::ScanResult VirusScanner::scan(const std::vector<uint8_t>& data) {
    ScanResult result;
    if (data.size() > max_scan_size_) {
        result.error = true;
        result.error_message = "File exceeds configured ClamAV scan size";
        return result;
    }

    const auto start = std::chrono::steady_clock::now();
    const auto response = send_instream(data);
    result.scan_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    if (!response) {
        result.error = true;
        result.error_message = last_error_.empty() ? "No response from clamd" : last_error_;
        return result;
    }

    result = parse_scan_result(*response);
    result.scan_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    return result;
}

VirusScanner::ScanResult VirusScanner::parse_scan_result(const std::string& response) {
    const std::string trimmed = trim_response(response);
    ScanResult result;

    if (trimmed.empty()) {
        result.error = true;
        result.error_message = "Empty response from clamd";
        return result;
    }

    if (trimmed.size() >= 3 && trimmed.ends_with("OK")) {
        result.clean = true;
        return result;
    }

    if (trimmed.size() >= 5 && trimmed.ends_with("FOUND")) {
        result.clean = false;
        const auto colon = trimmed.rfind(':');
        const auto found = trimmed.rfind(" FOUND");
        if (colon != std::string::npos && found != std::string::npos && colon + 2 <= found) {
            result.virus_name = trimmed.substr(colon + 2, found - (colon + 2));
        } else {
            result.virus_name = trimmed;
        }
        return result;
    }

    result.error = true;
    const auto colon = trimmed.rfind(':');
    if (colon != std::string::npos && colon + 2 < trimmed.size()) {
        result.error_message = trimmed.substr(colon + 2);
    } else {
        result.error_message = trimmed;
    }
    return result;
}

VirusScanner::SocketHandle VirusScanner::connect_to_daemon() {
    last_error_.clear();

#ifdef _WIN32
    ensure_winsock_initialized();
#endif

    if (type_ == ConnectionType::UnixSocket) {
#ifdef _WIN32
        last_error_ = "Unix socket ClamAV connections are not supported on Windows";
        return kInvalidSocket;
#else
        if (socket_path_.empty()) {
            last_error_ = "Empty clamd Unix socket path";
            return kInvalidSocket;
        }

        const int sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) {
            last_error_ = std::strerror(errno);
            return kInvalidSocket;
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        if (socket_path_.size() >= sizeof(addr.sun_path)) {
            last_error_ = "ClamAV socket path too long";
            ::close(sock);
            return kInvalidSocket;
        }
        std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

        if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            last_error_ = std::strerror(errno);
            ::close(sock);
            return kInvalidSocket;
        }

        return sock;
#endif
    }

    if (host_.empty() || port_ == 0) {
        last_error_ = "Invalid clamd TCP endpoint";
        return kInvalidSocket;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    const auto port_str = std::to_string(port_);
    if (::getaddrinfo(host_.c_str(), port_str.c_str(), &hints, &result) != 0) {
        last_error_ = "Failed to resolve clamd host";
        return kInvalidSocket;
    }

    SocketHandle connected = kInvalidSocket;
    for (auto* it = result; it != nullptr; it = it->ai_next) {
#ifdef _WIN32
        const SOCKET sock = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (sock == INVALID_SOCKET) {
            continue;
        }
        if (::connect(sock, it->ai_addr, static_cast<int>(it->ai_addrlen)) == 0) {
            connected = static_cast<SocketHandle>(sock);
            break;
        }
        ::closesocket(sock);
#else
        const int sock = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (sock < 0) {
            continue;
        }
        if (::connect(sock, it->ai_addr, it->ai_addrlen) == 0) {
            connected = sock;
            break;
        }
        ::close(sock);
#endif
    }

    freeaddrinfo(result);

    if (!valid_socket(connected)) {
        last_error_ = "Failed to connect to clamd";
    }
    return connected;
}

void VirusScanner::close_socket(SocketHandle sock) {
    if (!valid_socket(sock)) {
        return;
    }
#ifdef _WIN32
    ::closesocket(static_cast<SOCKET>(sock));
#else
    ::close(sock);
#endif
}

bool VirusScanner::set_socket_timeout(SocketHandle sock, std::chrono::milliseconds timeout) {
#ifdef _WIN32
    const DWORD timeout_ms = static_cast<DWORD>(timeout.count());
    const auto opt = reinterpret_cast<const char*>(&timeout_ms);
    return setsockopt(static_cast<SOCKET>(sock), SOL_SOCKET, SO_RCVTIMEO, opt, sizeof(timeout_ms)) == 0 &&
           setsockopt(static_cast<SOCKET>(sock), SOL_SOCKET, SO_SNDTIMEO, opt, sizeof(timeout_ms)) == 0;
#else
    timeval tv{};
    tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
    tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0 &&
           setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0;
#endif
}

std::optional<std::string> VirusScanner::send_command(const std::string& cmd) {
    auto sock = connect_to_daemon();
    if (!valid_socket(sock)) {
        return std::nullopt;
    }

    if (!set_socket_timeout(sock, timeout_)) {
        last_error_ = "Failed to set clamd socket timeout";
        close_socket(sock);
        return std::nullopt;
    }

    std::string framed = cmd;
    if (framed.empty() || framed.back() != '\0') {
        framed.push_back('\0');
    }

    if (!send_all(sock, reinterpret_cast<const uint8_t*>(framed.data()), framed.size())) {
        last_error_ = "Failed to send command to clamd";
        close_socket(sock);
        return std::nullopt;
    }

    auto response = receive_response(sock);
    if (!response) {
        last_error_ = "Failed to receive response from clamd";
    }
    close_socket(sock);
    return response;
}

std::optional<std::string> VirusScanner::send_instream(const std::vector<uint8_t>& data) {
    auto sock = connect_to_daemon();
    if (!valid_socket(sock)) {
        return std::nullopt;
    }

    if (!set_socket_timeout(sock, timeout_)) {
        last_error_ = "Failed to set clamd socket timeout";
        close_socket(sock);
        return std::nullopt;
    }

    static constexpr std::string_view command = "zINSTREAM";
    if (!send_all(sock, reinterpret_cast<const uint8_t*>(command.data()), command.size()) ||
        !send_all(sock, reinterpret_cast<const uint8_t*>("\0"), 1)) {
        last_error_ = "Failed to start INSTREAM scan";
        close_socket(sock);
        return std::nullopt;
    }

    static constexpr size_t kChunkBytes = 64 * 1024;
    size_t offset = 0;
    while (offset < data.size()) {
        const auto chunk_size = static_cast<uint32_t>(std::min(kChunkBytes, data.size() - offset));
        const auto network_size = htonl(chunk_size);
        if (!send_all(sock, reinterpret_cast<const uint8_t*>(&network_size), sizeof(network_size)) ||
            !send_all(sock, data.data() + offset, chunk_size)) {
            last_error_ = "Failed to stream payload to clamd";
            close_socket(sock);
            return std::nullopt;
        }
        offset += chunk_size;
    }

    const uint32_t terminator = 0;
    if (!send_all(sock, reinterpret_cast<const uint8_t*>(&terminator), sizeof(terminator))) {
        last_error_ = "Failed to terminate clamd INSTREAM payload";
        close_socket(sock);
        return std::nullopt;
    }

    auto response = receive_response(sock);
    if (!response) {
        last_error_ = "Failed to receive INSTREAM response from clamd";
    }
    close_socket(sock);
    return response;
}

VirusScannerManager& VirusScannerManager::instance() {
    static VirusScannerManager instance;
    return instance;
}

bool VirusScannerManager::initialize(const std::string& socket_path) {
    configured_ = true;
    scanner_ = std::make_unique<VirusScanner>(socket_path);
    available_ = scanner_->is_available();

    if (available_) {
        auto version = scanner_->get_version();
        if (version) {
            spdlog::info("VirusScanner: Connected to ClamAV {}", *version);
        }
    } else {
        spdlog::error("VirusScanner: ClamAV not available at {} ({})",
                      socket_path,
                      scanner_->last_error().empty() ? "no error detail" : scanner_->last_error());
    }

    return available_;
}

bool VirusScannerManager::initialize(const std::string& host, uint16_t port) {
    configured_ = true;
    scanner_ = std::make_unique<VirusScanner>(host, port);
    available_ = scanner_->is_available();

    if (available_) {
        auto version = scanner_->get_version();
        if (version) {
            spdlog::info("VirusScanner: Connected to ClamAV {} at {}:{}",
                         *version, host, port);
        }
    } else {
        spdlog::error("VirusScanner: ClamAV not available at {}:{} ({})",
                      host,
                      port,
                      scanner_->last_error().empty() ? "no error detail" : scanner_->last_error());
    }

    return available_;
}

bool VirusScannerManager::is_available() const {
    return enabled_ && configured_ && available_;
}

VirusScanner::ScanResult VirusScannerManager::scan(const std::vector<uint8_t>& data) {
    if (!configured_) {
        VirusScanner::ScanResult result;
        result.clean = true;
        return result;
    }

    if (!enabled_) {
        VirusScanner::ScanResult result;
        result.clean = true;
        return result;
    }

    if (!available_ || !scanner_) {
        VirusScanner::ScanResult result;
        result.error = true;
        result.error_message = "ClamAV is configured but unavailable";
        return result;
    }

    auto result = scanner_->scan(data);

    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.files_scanned++;
    stats_.total_scan_time += result.scan_time;
    if (!result.clean && !result.error) {
        stats_.threats_found++;
    }
    if (result.error) {
        stats_.scan_errors++;
    }

    return result;
}

VirusScannerManager::Stats VirusScannerManager::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void VirusScannerManager::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = Stats{};
}

} // namespace grotto::security
