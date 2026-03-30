#include "ui/terminal_image.hpp"

#include <curl/curl.h>
#include <stb_image.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#endif

namespace grotto::ui {

namespace {

struct FetchedImage {
    std::string content_type;
    std::vector<unsigned char> bytes;
};

TerminalGraphicsMode g_terminal_graphics_mode = TerminalGraphicsMode::Auto;

struct QuantizedImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> palette_rgb;
    std::vector<uint8_t> indices;
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

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool sixel_supported_environment() {
    if (std::getenv("WT_SESSION")) {
        return true;
    }
    if (const char* term = std::getenv("TERM")) {
        const std::string value = to_lower_ascii(term);
        if (value.find("sixel") != std::string::npos) {
            return true;
        }
    }
    if (const char* colorterm = std::getenv("COLORTERM")) {
        const std::string value = to_lower_ascii(colorterm);
        if (value.find("sixel") != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool kitty_supported_environment() {
    if (const char* term_program = std::getenv("TERM_PROGRAM")) {
        if (std::string(term_program) == "WezTerm") {
            return true;
        }
    }

    if (std::getenv("KITTY_WINDOW_ID")) {
        return true;
    }

    if (const char* term = std::getenv("TERM")) {
        std::string value = to_lower_ascii(term);
        if (value.find("kitty") != std::string::npos) {
            return true;
        }
    }

    return false;
}

TerminalInlineProtocol detect_protocol() {
    if (g_terminal_graphics_mode == TerminalGraphicsMode::Off) {
        return TerminalInlineProtocol::None;
    }

    if (const char* term_program = std::getenv("TERM_PROGRAM")) {
        std::string value = term_program;
        if (value == "iTerm.app") {
            return TerminalInlineProtocol::ITerm2;
        }
        if (value == "WezTerm") {
            return TerminalInlineProtocol::Kitty;
        }
    }

    if (kitty_supported_environment()) {
        return TerminalInlineProtocol::Kitty;
    }

    if (sixel_supported_environment()) {
        return TerminalInlineProtocol::Sixel;
    }

    return TerminalInlineProtocol::None;
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

bool show_kitty_rgba(const unsigned char* rgba, int width, int height) {
    if (!rgba || width <= 0 || height <= 0) {
        return false;
    }

    const size_t raw_size = static_cast<size_t>(width * height * 4);
    std::string encoded = base64_encode(rgba, raw_size);

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

void move_cursor_to_cell(int x, int y) {
    const int col = std::max(1, x + 1);
    const int row = std::max(1, y + 1);
    std::cout << "\033[" << row << ";" << col << "H";
}

void clear_kitty_inline_images() {
    std::cout << "\033_Ga=d,d=A\033\\";
}

bool draw_kitty_rgba_at(const unsigned char* rgba,
                        int width,
                        int height,
                        int x,
                        int y,
                        int columns,
                        int rows,
                        int z_index) {
    if (!rgba || width <= 0 || height <= 0 || columns <= 0 || rows <= 0) {
        return false;
    }

    const size_t raw_size = static_cast<size_t>(width * height * 4);
    std::string encoded = base64_encode(rgba, raw_size);

    constexpr size_t kChunkSize = 4096;
    move_cursor_to_cell(x, y);
    for (size_t offset = 0; offset < encoded.size(); offset += kChunkSize) {
        const bool first = offset == 0;
        const bool more = offset + kChunkSize < encoded.size();
        std::string chunk = encoded.substr(offset, kChunkSize);

        std::cout << "\033_G";
        if (first) {
            std::cout << "a=T,f=32,s=" << width << ",v=" << height
                      << ",c=" << columns << ",r=" << rows
                      << ",z=" << z_index
                      << ",m=" << (more ? 1 : 0);
        } else {
            std::cout << "m=" << (more ? 1 : 0);
        }
        std::cout << ";" << chunk << "\033\\";
    }
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

    const bool ok = show_kitty_rgba(rgba, width, height);
    stbi_image_free(rgba);
    return ok;
}

std::vector<unsigned char> resample_rgba(const unsigned char* rgba,
                                         int width,
                                         int height,
                                         int max_width,
                                         int max_height,
                                         int* out_width,
                                         int* out_height) {
    if (!rgba || width <= 0 || height <= 0) {
        return {};
    }

    const float scale_x = static_cast<float>(max_width) / static_cast<float>(width);
    const float scale_y = static_cast<float>(max_height) / static_cast<float>(height);
    const float scale = std::min(1.0f, std::min(scale_x, scale_y));

    const int target_width = std::max(1, static_cast<int>(width * scale));
    const int target_height = std::max(1, static_cast<int>(height * scale));
    if (out_width) {
        *out_width = target_width;
    }
    if (out_height) {
        *out_height = target_height;
    }

    std::vector<unsigned char> out(static_cast<size_t>(target_width * target_height * 4), 0);
    for (int y = 0; y < target_height; ++y) {
        const int src_y = std::min(height - 1, y * height / target_height);
        for (int x = 0; x < target_width; ++x) {
            const int src_x = std::min(width - 1, x * width / target_width);
            const size_t src_idx = static_cast<size_t>((src_y * width + src_x) * 4);
            const size_t dst_idx = static_cast<size_t>((y * target_width + x) * 4);
            for (size_t c = 0; c < 4; ++c) {
                out[dst_idx + c] = rgba[src_idx + c];
            }
        }
    }

    return out;
}

uint8_t quantize_channel(uint8_t value) {
    return static_cast<uint8_t>((value * 5 + 127) / 255);
}

QuantizedImage quantize_rgba(const unsigned char* rgba, int width, int height) {
    QuantizedImage result;
    if (!rgba || width <= 0 || height <= 0) {
        return result;
    }

    result.width = width;
    result.height = height;
    result.indices.resize(static_cast<size_t>(width * height), 0);

    constexpr int kPaletteSide = 6;
    constexpr int kPaletteSize = kPaletteSide * kPaletteSide * kPaletteSide;
    result.palette_rgb.resize(static_cast<size_t>(kPaletteSize * 3), 0);
    for (int r = 0; r < kPaletteSide; ++r) {
        for (int g = 0; g < kPaletteSide; ++g) {
            for (int b = 0; b < kPaletteSide; ++b) {
                const int index = (r * kPaletteSide + g) * kPaletteSide + b;
                result.palette_rgb[static_cast<size_t>(index * 3 + 0)] =
                    static_cast<uint8_t>(r * 255 / (kPaletteSide - 1));
                result.palette_rgb[static_cast<size_t>(index * 3 + 1)] =
                    static_cast<uint8_t>(g * 255 / (kPaletteSide - 1));
                result.palette_rgb[static_cast<size_t>(index * 3 + 2)] =
                    static_cast<uint8_t>(b * 255 / (kPaletteSide - 1));
            }
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t src_idx = static_cast<size_t>((y * width + x) * 4);
            const uint8_t alpha = rgba[src_idx + 3];
            const uint8_t r = static_cast<uint8_t>((rgba[src_idx + 0] * alpha) / 255);
            const uint8_t g = static_cast<uint8_t>((rgba[src_idx + 1] * alpha) / 255);
            const uint8_t b = static_cast<uint8_t>((rgba[src_idx + 2] * alpha) / 255);
            const int index = (quantize_channel(r) * kPaletteSide + quantize_channel(g)) * kPaletteSide +
                              quantize_channel(b);
            result.indices[static_cast<size_t>(y * width + x)] = static_cast<uint8_t>(index);
        }
    }

    return result;
}

void append_sixel_run(std::string* output, char symbol, int count) {
    if (!output || count <= 0) {
        return;
    }
    if (count > 3) {
        *output += "!";
        *output += std::to_string(count);
        output->push_back(symbol);
        return;
    }
    output->append(static_cast<size_t>(count), symbol);
}

bool encode_sixel_image(const unsigned char* rgba,
                        int width,
                        int height,
                        std::string* output) {
    if (!rgba || width <= 0 || height <= 0 || !output) {
        return false;
    }

    const QuantizedImage image = quantize_rgba(rgba, width, height);
    if (image.indices.empty() || image.palette_rgb.empty()) {
        return false;
    }

    output->clear();
    *output += "\033Pq";

    const int palette_size = static_cast<int>(image.palette_rgb.size() / 3);
    for (int i = 0; i < palette_size; ++i) {
        const int r = static_cast<int>(image.palette_rgb[static_cast<size_t>(i * 3 + 0)]) * 100 / 255;
        const int g = static_cast<int>(image.palette_rgb[static_cast<size_t>(i * 3 + 1)]) * 100 / 255;
        const int b = static_cast<int>(image.palette_rgb[static_cast<size_t>(i * 3 + 2)]) * 100 / 255;
        *output += "#" + std::to_string(i) + ";2;" + std::to_string(r) + ";" +
                   std::to_string(g) + ";" + std::to_string(b);
    }

    std::vector<int> band_colors;
    std::vector<bool> used(static_cast<size_t>(palette_size), false);
    for (int band_y = 0; band_y < image.height; band_y += 6) {
        std::fill(used.begin(), used.end(), false);
        band_colors.clear();
        for (int dy = 0; dy < 6 && band_y + dy < image.height; ++dy) {
            for (int x = 0; x < image.width; ++x) {
                const uint8_t index =
                    image.indices[static_cast<size_t>((band_y + dy) * image.width + x)];
                if (!used[index]) {
                    used[index] = true;
                    band_colors.push_back(index);
                }
            }
        }

        bool first_color = true;
        for (int color : band_colors) {
            if (!first_color) {
                output->push_back('$');
            }
            first_color = false;
            *output += "#" + std::to_string(color);

            char current_symbol = 0;
            int run_length = 0;
            for (int x = 0; x < image.width; ++x) {
                int bits = 0;
                for (int dy = 0; dy < 6 && band_y + dy < image.height; ++dy) {
                    if (image.indices[static_cast<size_t>((band_y + dy) * image.width + x)] == color) {
                        bits |= (1 << dy);
                    }
                }
                const char symbol = static_cast<char>(63 + bits);
                if (run_length == 0) {
                    current_symbol = symbol;
                    run_length = 1;
                } else if (symbol == current_symbol) {
                    ++run_length;
                } else {
                    append_sixel_run(output, current_symbol, run_length);
                    current_symbol = symbol;
                    run_length = 1;
                }
            }
            append_sixel_run(output, current_symbol, run_length);
        }
        output->push_back('-');
    }

    return true;
}

bool show_sixel_image(const unsigned char* rgba, int width, int height) {
    constexpr int kMaxViewerWidth = 320;
    constexpr int kMaxViewerHeight = 240;
    int scaled_width = 0;
    int scaled_height = 0;
    const auto scaled = resample_rgba(rgba, width, height,
                                      kMaxViewerWidth, kMaxViewerHeight,
                                      &scaled_width, &scaled_height);
    if (scaled.empty()) {
        return false;
    }

    std::string encoded;
    if (!encode_sixel_image(scaled.data(), scaled_width, scaled_height, &encoded)) {
        return false;
    }

    std::cout << "\n" << encoded << "\033\\\n";
    return true;
}

bool draw_sixel_image_at(const unsigned char* rgba,
                         int width,
                         int height,
                         int x,
                         int y,
                         int columns,
                         int rows) {
    if (!rgba || width <= 0 || height <= 0 || columns <= 0 || rows <= 0) {
        return false;
    }

    constexpr int kApproxCellWidthPx = 8;
    constexpr int kApproxCellHeightPx = 16;
    const int target_width = std::max(1, columns * kApproxCellWidthPx);
    const int target_height = std::max(2, rows * kApproxCellHeightPx);

    int scaled_width = 0;
    int scaled_height = 0;
    const auto scaled = resample_rgba(
        rgba, width, height, target_width, target_height, &scaled_width, &scaled_height);
    if (scaled.empty()) {
        return false;
    }

    std::string encoded;
    if (!encode_sixel_image(scaled.data(), scaled_width, scaled_height, &encoded)) {
        return false;
    }

    move_cursor_to_cell(x, y);
    std::cout << encoded << "\033\\";
    return true;
}

void wait_for_viewer_close() {
#ifdef _WIN32
    (void)_getch();
#else
    (void)std::getchar();
#endif
}

bool show_sixel_image(const FetchedImage& image) {
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

    const bool ok = show_sixel_image(rgba, width, height);
    stbi_image_free(rgba);
    return ok;
}

} // namespace

TerminalGraphicsMode parse_terminal_graphics_mode(std::string_view value) {
    std::string normalized(value);
    normalized = to_lower_ascii(normalized);
    if (normalized == "off") {
        return TerminalGraphicsMode::Off;
    }
    if (normalized == "viewer-only") {
        return TerminalGraphicsMode::ViewerOnly;
    }
    return TerminalGraphicsMode::Auto;
}

void configure_terminal_graphics(TerminalGraphicsMode mode) {
    g_terminal_graphics_mode = mode;
}

TerminalInlineProtocol terminal_inline_protocol() {
    return detect_protocol();
}

TerminalInlineProtocol terminal_inline_protocol_for_compositor() {
    if (g_terminal_graphics_mode != TerminalGraphicsMode::Auto) {
        return TerminalInlineProtocol::None;
    }

    const TerminalInlineProtocol protocol = detect_protocol();
    if (protocol == TerminalInlineProtocol::Kitty ||
        protocol == TerminalInlineProtocol::Sixel) {
        return protocol;
    }

    return TerminalInlineProtocol::None;
}

bool terminal_inline_images_supported() {
    return terminal_inline_protocol_for_compositor() != TerminalInlineProtocol::None;
}

bool terminal_uses_compact_image_preview() {
    return terminal_inline_protocol_for_compositor() == TerminalInlineProtocol::Sixel;
}

bool terminal_inline_native_graphics_enabled() {
    return terminal_inline_protocol_for_compositor() != TerminalInlineProtocol::None;
}

bool display_inline_image_from_url(ftxui::ScreenInteractive& screen,
                                   const std::string& url) {
    const TerminalInlineProtocol protocol = detect_protocol();
    if (protocol == TerminalInlineProtocol::None) {
        return false;
    }

    auto image = fetch_image_bytes(url);
    if (!image) {
        return false;
    }

    bool shown = false;
    auto show = screen.WithRestoredIO([&] {
        if (protocol == TerminalInlineProtocol::ITerm2) {
            shown = show_iterm2_image(*image);
        } else if (protocol == TerminalInlineProtocol::Kitty) {
            shown = show_kitty_image(*image);
        } else {
            shown = show_sixel_image(*image);
        }
        if (!shown) {
            return;
        }
        std::cout << "Press any key to return..." << std::flush;
        wait_for_viewer_close();
    });
    show();
    return shown;
}

bool display_inline_image(ftxui::ScreenInteractive& screen,
                          const InlineImageThumbnail& thumbnail,
                          const std::string& title) {
    const TerminalInlineProtocol protocol = detect_protocol();
    if (protocol == TerminalInlineProtocol::None ||
        thumbnail.rgba.empty() ||
        thumbnail.width <= 0 ||
        thumbnail.height <= 0) {
        return false;
    }

    bool shown = false;
    auto show = screen.WithRestoredIO([&] {
        if (!title.empty()) {
            std::cout << title << "\n";
        }

        if (protocol == TerminalInlineProtocol::Sixel) {
            shown = show_sixel_image(thumbnail.rgba.data(), thumbnail.width, thumbnail.height);
        } else if (protocol == TerminalInlineProtocol::Kitty) {
            shown = show_kitty_rgba(thumbnail.rgba.data(), thumbnail.width, thumbnail.height);
        } else {
            shown = false;
        }

        if (!shown) {
            return;
        }
        std::cout << "Press any key to return..." << std::flush;
        wait_for_viewer_close();
    });
    show();
    return shown;
}

void clear_inline_graphics_layer() {
    if (kitty_supported_environment()) {
        clear_kitty_inline_images();
        std::cout.flush();
    }
}

void draw_inline_graphics_commands(const std::vector<GraphicsDrawCommand>& commands) {
    if (commands.empty()) {
        return;
    }

    bool has_kitty = false;
    for (const auto& cmd : commands) {
        if (cmd.backend == GraphicsBackendKind::Kitty) {
            has_kitty = true;
            break;
        }
    }
    if (has_kitty) {
        clear_kitty_inline_images();
    }

    for (const auto& cmd : commands) {
        if (!cmd.image ||
            cmd.image->rgba.empty() ||
            cmd.image->width <= 0 ||
            cmd.image->height <= 0 ||
            cmd.width <= 0 ||
            cmd.height <= 0) {
            continue;
        }

        if (cmd.backend == GraphicsBackendKind::Sixel) {
            draw_sixel_image_at(cmd.image->rgba.data(),
                                cmd.image->width,
                                cmd.image->height,
                                cmd.viewport_x,
                                cmd.viewport_y,
                                cmd.width,
                                cmd.height);
            continue;
        }

        if (cmd.backend == GraphicsBackendKind::Kitty) {
            draw_kitty_rgba_at(cmd.image->rgba.data(),
                               cmd.image->width,
                               cmd.image->height,
                               cmd.viewport_x,
                               cmd.viewport_y,
                               cmd.width,
                               cmd.height,
                               cmd.z_index);
        }
    }
    std::cout.flush();
}

} // namespace grotto::ui
