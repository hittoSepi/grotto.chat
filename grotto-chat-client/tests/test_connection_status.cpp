#include "connection_status.hpp"
#include "i18n/strings.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("disconnect reasons are sanitized for common reconnect failures", "[connection]") {
    REQUIRE(grotto::sanitize_disconnect_reason("Certificate pinning check failed") ==
            "TLS certificate pin check failed");
    REQUIRE(grotto::sanitize_disconnect_reason("handshake: stream truncated") ==
            "TLS handshake failed");
    REQUIRE(grotto::sanitize_disconnect_reason("host not found [system:11001]") ==
            "could not resolve server address");
    REQUIRE(grotto::sanitize_disconnect_reason("No such host is known.") ==
            "could not resolve server address");
    REQUIRE(grotto::sanitize_disconnect_reason("  end of file  ") ==
            "connection closed by server");
}

TEST_CASE("disconnect sanitizer trims noisy detail from generic errors", "[connection]") {
    const std::string original =
        "socket exploded unexpectedly [asio.ssl:1234 with extra context that should be trimmed]";
    REQUIRE(grotto::sanitize_disconnect_reason(original) == "socket exploded unexpectedly");
}

TEST_CASE("connection summaries expose user-facing phases", "[connection]") {
    grotto::i18n::set_language("en");

    REQUIRE(grotto::connection_summary_for_phase("Resolving server address") == "resolving");
    REQUIRE(grotto::connection_summary_for_phase("TCP connected") == "tls handshake");
    REQUIRE(grotto::connection_summary_for_phase("Certificate verified") == "verifying cert");
    REQUIRE(grotto::connection_summary_for_phase("Auth response sent") == "authenticating");
    REQUIRE(grotto::connection_summary_for_phase(
                "reconnect in 8s (attempt 2, total wait so far ~9s)") == "retry in 8s");
    REQUIRE(grotto::connection_summary_for_phase("Server address resolved").empty());
}

TEST_CASE("auth failure details are normalized for identity recovery cases", "[connection]") {
    grotto::i18n::set_language("en");

    REQUIRE(
        grotto::normalize_auth_failure_detail("Identity key mismatch and password incorrect") ==
        "Local identity does not match the server record, and the recovery passkey was incorrect. Use the original passkey or press CLEAR CREDS.");
    REQUIRE(
        grotto::normalize_auth_failure_detail(
            "Identity key mismatch. Set a password with /password to enable key recovery.") ==
        "Local identity does not match the server record. Set a recovery password with /password on a successful login before using key recovery.");
    REQUIRE(grotto::normalize_auth_failure_detail("Nickname 'hitto' is already in use.") ==
            "Nickname 'hitto' is already in use.");
}
