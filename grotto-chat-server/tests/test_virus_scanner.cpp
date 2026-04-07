#include <catch2/catch_test_macros.hpp>

#include "security/virus_scanner.hpp"

#include <boost/asio.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace {

class FakeClamdServer {
public:
    explicit FakeClamdServer(std::string instream_response)
        : acceptor_(ioc_, {boost::asio::ip::tcp::v4(), 0})
        , port_(acceptor_.local_endpoint().port())
        , instream_response_(std::move(instream_response)) {
        thread_ = std::thread([this] { run(); });
    }

    ~FakeClamdServer() {
        running_ = false;
        boost::system::error_code ec;
        acceptor_.close(ec);

        try {
            boost::asio::io_context wake_ioc;
            boost::asio::ip::tcp::socket socket(wake_ioc);
            socket.connect({boost::asio::ip::address_v4::loopback(), port_}, ec);
        } catch (...) {
        }

        if (thread_.joinable()) {
            thread_.join();
        }
    }

    uint16_t port() const { return port_; }

private:
    void run() {
        while (running_) {
            boost::system::error_code ec;
            boost::asio::ip::tcp::socket socket(ioc_);
            acceptor_.accept(socket, ec);
            if (ec) {
                continue;
            }
            handle_connection(socket);
        }
    }

    static std::optional<std::string> read_command(boost::asio::ip::tcp::socket& socket) {
        std::string command;
        char ch = '\0';
        boost::system::error_code ec;
        while (true) {
            const auto read = socket.read_some(boost::asio::buffer(&ch, 1), ec);
            if (ec || read == 0) {
                return std::nullopt;
            }
            if (ch == '\0') {
                return command;
            }
            command.push_back(ch);
        }
    }

    static bool read_exact(boost::asio::ip::tcp::socket& socket, void* data, size_t size) {
        boost::system::error_code ec;
        boost::asio::read(socket, boost::asio::buffer(data, size), ec);
        return !ec;
    }

    void handle_connection(boost::asio::ip::tcp::socket& socket) {
        const auto command = read_command(socket);
        if (!command) {
            return;
        }

        if (*command == "zPING") {
            const std::string response = "PONG\0";
            boost::asio::write(socket, boost::asio::buffer(response));
            return;
        }

        if (*command == "zVERSION") {
            const std::string response = "ClamAV 1.4.0/test\0";
            boost::asio::write(socket, boost::asio::buffer(response));
            return;
        }

        if (*command != "zINSTREAM") {
            return;
        }

        std::vector<uint8_t> payload;
        while (true) {
            uint32_t network_size = 0;
            if (!read_exact(socket, &network_size, sizeof(network_size))) {
                return;
            }
            const auto chunk_size = ntohl(network_size);
            if (chunk_size == 0) {
                break;
            }
            const auto start = payload.size();
            payload.resize(start + chunk_size);
            if (!read_exact(socket, payload.data() + start, chunk_size)) {
                return;
            }
        }

        const std::string response = instream_response_ + '\0';
        boost::asio::write(socket, boost::asio::buffer(response));
    }

    boost::asio::io_context ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
    uint16_t port_ = 0;
    std::string instream_response_;
    std::atomic<bool> running_{true};
    std::thread thread_;
};

uint16_t reserve_unused_port() {
    boost::asio::io_context ioc;
    boost::asio::ip::tcp::acceptor acceptor(ioc, {boost::asio::ip::tcp::v4(), 0});
    const auto port = acceptor.local_endpoint().port();
    boost::system::error_code ec;
    acceptor.close(ec);
    return port;
}

std::vector<uint8_t> sample_payload() {
    return std::vector<uint8_t>{'g', 'r', 'o', 't', 't', 'o'};
}

} // namespace

TEST_CASE("virus scanner handles clean tcp scan flow", "[virus-scanner]") {
    FakeClamdServer server("stream: OK");

    grotto::security::VirusScanner scanner("127.0.0.1", server.port());
    scanner.set_timeout(std::chrono::milliseconds(1000));

    REQUIRE(scanner.is_available());

    const auto version = scanner.get_version();
    REQUIRE(version.has_value());
    CHECK(version->find("ClamAV") != std::string::npos);

    const auto result = scanner.scan(sample_payload());
    CHECK(result.clean);
    CHECK_FALSE(result.error);
}

TEST_CASE("virus scanner reports detected threats", "[virus-scanner]") {
    FakeClamdServer server("stream: Eicar-Test-Signature FOUND");

    grotto::security::VirusScanner scanner("127.0.0.1", server.port());
    scanner.set_timeout(std::chrono::milliseconds(1000));

    const auto result = scanner.scan(sample_payload());
    CHECK_FALSE(result.clean);
    CHECK_FALSE(result.error);
    CHECK(result.virus_name == "Eicar-Test-Signature");
}

TEST_CASE("configured but unavailable scanner fails closed", "[virus-scanner]") {
    auto& manager = grotto::security::VirusScannerManager::instance();
    manager.reset_stats();
    manager.set_enabled(true);

    const auto unused_port = reserve_unused_port();
    CHECK_FALSE(manager.initialize("127.0.0.1", unused_port));
    CHECK(manager.is_configured());
    CHECK_FALSE(manager.is_available());

    const auto result = manager.scan(sample_payload());
    CHECK(result.error);
    CHECK(result.error_message.find("configured but unavailable") != std::string::npos);
}
