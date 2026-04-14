#include "connection_status.hpp"

#include "i18n/strings.hpp"

#include <cctype>

namespace grotto {

namespace {

std::string trim_ascii_whitespace(std::string_view text) {
    size_t start = 0;
    size_t end = text.size();
    while (start < end &&
           std::isspace(static_cast<unsigned char>(text[start]))) {
        ++start;
    }
    while (end > start &&
           std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(start, end - start));
}

std::string ascii_lower_copy(std::string_view text) {
    std::string lowered(text);
    for (char& c : lowered) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lowered;
}

} // namespace

std::string sanitize_disconnect_reason(std::string_view reason) {
    std::string sanitized = trim_ascii_whitespace(reason);
    if (sanitized.empty()) {
        return "connection closed";
    }

    const std::string lowered = ascii_lower_copy(sanitized);
    if (lowered.find("multiple exceptions") != std::string::npos) {
        return "connection lost";
    }
    if (lowered.find("actively refused") != std::string::npos ||
        lowered.find("system:10061") != std::string::npos) {
        return "connection refused";
    }
    if (lowered.find("end of file") != std::string::npos) {
        return "connection closed by server";
    }
    if (lowered.find("connection reset by peer") != std::string::npos) {
        return "connection reset by peer";
    }
    if (lowered.find("certificate pinning check failed") != std::string::npos) {
        return "TLS certificate pin check failed";
    }
    if (lowered.find("handshake") != std::string::npos) {
        return "TLS handshake failed";
    }
    if (lowered.find("resolve") != std::string::npos ||
        lowered.find("host not found") != std::string::npos ||
        lowered.find("no such host is known") != std::string::npos ||
        lowered.find("name or service not known") != std::string::npos) {
        return "could not resolve server address";
    }

    const auto bracket = sanitized.find(" [");
    if (bracket != std::string::npos) {
        sanitized.erase(bracket);
    }

    constexpr size_t kMaxReasonLength = 120;
    if (sanitized.size() > kMaxReasonLength) {
        sanitized.resize(kMaxReasonLength);
        sanitized += "...";
    }
    return sanitized;
}

std::string connection_summary_for_phase(std::string_view phase) {
    using grotto::i18n::I18nKey;

    if (phase == "Resolving server address") {
        return i18n::tr(I18nKey::CONNECTION_SUMMARY_RESOLVING);
    }
    if (phase == "TCP connected" || phase == "TLS handshake done") {
        return i18n::tr(I18nKey::CONNECTION_SUMMARY_TLS);
    }
    if (phase == "Certificate verified") {
        return i18n::tr(I18nKey::CONNECTION_SUMMARY_VERIFYING_CERT);
    }
    if (phase == "HELLO sent" ||
        phase == "Auth challenge received" ||
        phase == "Auth response sent") {
        return i18n::tr(I18nKey::CONNECTION_SUMMARY_AUTHENTICATING);
    }

    constexpr std::string_view kReconnectPrefix = "reconnect in ";
    if (phase.substr(0, kReconnectPrefix.size()) == kReconnectPrefix) {
        const auto seconds_begin = kReconnectPrefix.size();
        const auto seconds_end = phase.find('s', seconds_begin);
        if (seconds_end != std::string_view::npos && seconds_end > seconds_begin) {
            return i18n::tr(
                I18nKey::CONNECTION_SUMMARY_RETRY_IN,
                std::string(phase.substr(seconds_begin, seconds_end - seconds_begin)));
        }
    }

    return {};
}

std::string normalize_auth_failure_detail(std::string_view detail) {
    const std::string trimmed = trim_ascii_whitespace(detail);
    if (trimmed.empty()) {
        return {};
    }

    const std::string lowered = ascii_lower_copy(trimmed);
    if (lowered == "identity key mismatch and password incorrect") {
        return i18n::tr(i18n::I18nKey::AUTH_FAILED_IDENTITY_PASSWORD_MISMATCH);
    }

    if (lowered ==
        "identity key mismatch. set a password with /password to enable key recovery.") {
        return i18n::tr(i18n::I18nKey::AUTH_FAILED_IDENTITY_RECOVERY_REQUIRED);
    }

    return trimmed;
}

} // namespace grotto
