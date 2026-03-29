#include "ui/terminal_image.hpp"

#include <curl/curl.h>
#include <stb_image.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace grotto::ui {

namespace {

enum class TerminalImageProtocol {
    None,
    Kitty,
    ITerm2,
};

struct FetchedImage {
    std::string content_type;
    std::vector<unsigned char> bytes;
};

std::string base64_encode(const unsigned char* data, size_t len) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        const uint32_t octet_a = data[i];
        const uint32_t octet_b = (i + 1 < len) ? data[i + 1] : 0;
        const uint32_t octet_c = (i + 2 < len) ? data[i + 2] : 0;
        const uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out.push_back(kAlphabet[(triple >> 18) & 0x3F]);
        out.push_back(kAlphabet[(triple >> 12) & 0x3F]);
        out.push_back(i + 1 < len ? kAlphabet[(triple >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < len ? kAlphabet[triple & 0x3F] : '=');
    }
    return out;
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::vector<unsigned char>*>(userdata);
    size_t bytes = size * nmemb;
    constexpr size_t kMaxImageBytes = 10 * 1024 * 1024;
    if (out->size() + bytes > kMaxImageBytes) {
        bytes = kMaxImageBytes - out->size();
    }
    out->insert(out->end(), reinterpret_cast<unsigned char*>(ptr),
                reinterpret_cast<unsigned char*>(ptr) + bytes);
    return bytes;
}

TerminalImageProtocol detect_protocol() {
    if (const char* term_program = std::getenv("TERM_PROGRAM")) {
        std::string value = term_program;
        if (value == "iTerm.app") {
            return TerminalImageProtocol::ITerm2;
        }
        if (value == "WezTerm") {
            return TerminalImageProtocol::Kitty;
        }
    }

    if (std::getenv("KITTY_WINDOW_ID")) {
        return TerminalImageProtocol::Kitty;
    }

    if (const char* term = std::getenv("TERM")) {
        std::string value = term;
        if (value.find("kitty") != std::string::npos) {
            return TerminalImageProtocol::Kitty;
        }
    }

    return TerminalImageProtocol::None;
}

std::optional<FetchedImage> fetch_image_bytes(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return std::nullopt;
    }

    std::vector<unsigned char> body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");

    CURLcode rc = curl_easy_perform(curl);
    char* content_type_raw = nullptr;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type_raw);
    std::string content_type = content_type_raw ? content_type_raw : "";
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK || body.empty()) {
        return std::nullopt;
    }

    std::string lower = content_type;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (!lower.empty() && !lower.starts_with("image/")) {
        return std::nullopt;
    }

    return FetchedImage{content_type, std::move(body)};
}

bool show_iterm2_image(const FetchedImage& image) {
    std::string encoded = base64_encode(image.bytes.data(), image.bytes.size());
    std::cout << "\n\033]1337;File=inline=1;width=auto;height=auto;preserveAspectRatio=1:"
              << encoded << "\a\n";
    return true;
}

bool show_kitty_image(const FetchedImage& image) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* rgba = stbi_load_from_memory(image.bytes.data(),
                                          static_cast<int>(image.bytes.size()),
                                          &width, &height, &channels, 4);
    if (!rgba || width <= 0 || height <= 0) {
        if (rgba) {
            stbi_image_free(rgba);
        }
        return false;
    }

    const size_t raw_size = static_cast<size_t>(width * height * 4);
    std::string encoded = base64_encode(rgba, raw_size);
    stbi_image_free(rgba);

    constexpr size_t kChunkSize = 4096;
    std::cout << "\n";
    for (size_t offset = 0; offset < encoded.size(); offset += kChunkSize) {
        const bool first = offset == 0;
        const bool more = offset + kChunkSize < encoded.size();
        std::string chunk = encoded.substr(offset, kChunkSize);

        std::cout << "\033_G";
        if (first) {
            std::cout << "a=T,f=32,s=" << width << ",v=" << height << ",m=" << (more ? 1 : 0);
        } else {
            std::cout << "m=" << (more ? 1 : 0);
        }
        std::cout << ";" << chunk << "\033\\";
    }
    std::cout << "\n";
    return true;
}

} // namespace

bool terminal_inline_images_supported() {
    return detect_protocol() != TerminalImageProtocol::None;
}

bool display_inline_image_from_url(ftxui::ScreenInteractive& screen,
                                   const std::string& url) {
    const TerminalImageProtocol protocol = detect_protocol();
    if (protocol == TerminalImageProtocol::None) {
        return false;
    }

    auto image = fetch_image_bytes(url);
    if (!image) {
        return false;
    }

    bool shown = false;
    auto show = screen.WithRestoredIO([&] {
        shown = (protocol == TerminalImageProtocol::ITerm2)
            ? show_iterm2_image(*image)
            : show_kitty_image(*image);
        if (!shown) {
            return;
        }
        std::cout << "Press Enter to return..." << std::flush;
        std::string line;
        std::getline(std::cin, line);
    });
    show();
    return shown;
}

} // namespace grotto::ui
