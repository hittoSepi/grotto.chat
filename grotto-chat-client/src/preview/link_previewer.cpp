#include "preview/link_previewer.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <regex>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace grotto {

namespace {

std::string to_lower_ascii(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool supports_utf8_blocks() {
#ifdef _WIN32
    return true;
#else
    const char* lang = std::getenv("LC_ALL");
    if (!lang || !*lang) {
        lang = std::getenv("LANG");
    }
    if (!lang) {
        return false;
    }
    return std::string(lang).find("UTF-8") != std::string::npos ||
           std::string(lang).find("utf8") != std::string::npos;
#endif
}

std::string intensity_to_symbol(float intensity, bool utf8_blocks) {
    static const std::vector<std::string> kAsciiRamp = {
        " ", ".", ":", "-", "=", "+", "*", "#", "%", "@"
    };
    static const std::vector<std::string> kUtf8Ramp = {
        " ", "░", "▒", "▓", "█"
    };

    const auto& ramp = utf8_blocks ? kUtf8Ramp : kAsciiRamp;
    float clamped = std::clamp(intensity, 0.0f, 1.0f);
    size_t index = static_cast<size_t>(std::round(clamped * static_cast<float>(ramp.size() - 1)));
    return ramp[index];
}

} // namespace

LinkPreviewer::LinkPreviewer(db::LocalStore& store, int fetch_timeout_s, int max_cache,
                             bool inline_images, int image_columns, int image_rows)
    : store_(store)
    , fetch_timeout_s_(fetch_timeout_s)
    , max_cache_(max_cache)
    , inline_images_(inline_images)
    , image_columns_(image_columns)
    , image_rows_(image_rows)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

LinkPreviewer::~LinkPreviewer() {
    stop();
    curl_global_cleanup();
}

void LinkPreviewer::start() {
    stopping_ = false;
    thread_ = std::thread([this] { worker_loop(); });
}

void LinkPreviewer::stop() {
    {
        std::lock_guard lk(mu_);
        stopping_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void LinkPreviewer::fetch(const std::string& url, PreviewCallback callback) {
    // Check cache first
    if (!is_likely_image_url(url)) {
        auto cached = store_.get_cached_preview(url);
        if (cached) {
            PreviewResult r;
            r.url         = url;
            r.title       = cached->title;
            r.description = cached->description;
            r.success     = true;
            callback(std::move(r));
            return;
        }
    }

    {
        std::lock_guard lk(mu_);
        queue_.push({ url, std::move(callback) });
    }
    cv_.notify_one();
}

void LinkPreviewer::worker_loop() {
    while (true) {
        FetchJob job;
        {
            std::unique_lock lk(mu_);
            cv_.wait(lk, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) break;
            job = std::move(queue_.front());
            queue_.pop();
        }

        auto result = fetch_sync(job.url);

        if (result.success && !result.is_image) {
            store_.cache_preview(job.url, result.title, result.description);
        }

        job.callback(std::move(result));
    }
}

size_t LinkPreviewer::write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    size_t bytes = size * nmemb;

    // Cap total body size
    if (body->size() >= kMaxBodyBytes) {
        return 0;  // Signal abort — we have enough
    }
    if (body->size() + bytes > kMaxBodyBytes) {
        bytes = kMaxBodyBytes - body->size();
    }
    body->append(ptr, bytes);

    // Abort once </head> is seen — OG tags only appear in <head>
    if (body->find("</head>") != std::string::npos ||
        body->find("</HEAD>") != std::string::npos) {
        return 0;  // Signal abort to curl
    }

    return size * nmemb;
}

PreviewResult LinkPreviewer::fetch_sync(const std::string& url) {
    PreviewResult result;
    result.url = url;

    // Only allow http/https
    if (url.find("http://") != 0 && url.find("https://") != 0) {
        return result;
    }

    CURL* curl = curl_easy_init();
    if (!curl) return result;

    std::string body;
    body.reserve(64 * 1024);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(kMaxRedirects));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(fetch_timeout_s_));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    // Stop once we have enough to find OG tags
    // (write_callback caps at kMaxBodyBytes)

    // Add separate connect timeout for faster failure on unreachable hosts
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    CURLcode rc = curl_easy_perform(curl);
    char* content_type_raw = nullptr;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type_raw);
    std::string content_type = content_type_raw ? content_type_raw : "";
    curl_easy_cleanup(curl);

    // CURLE_WRITE_ERROR is expected when we abort early after finding </head>
    if (rc != CURLE_OK && rc != CURLE_WRITE_ERROR) {
        spdlog::debug("curl fetch failed for {}: {}", url, curl_easy_strerror(rc));
        return result;
    }

    if (inline_images_ &&
        (is_likely_image_url(url) || to_lower_ascii(content_type).starts_with("image/"))) {
        result.is_image = true;
        result.title = "[image] " + url;
        result.description = content_type.empty() ? "direct image link" : content_type;
        result.image_preview = render_image_preview(url, body, content_type);
        if (result.image_preview.empty()) {
            result.description += (result.description.empty() ? "" : " ");
            result.description += "(inline thumbnail unavailable)";
        }
        result.success = true;
        return result;
    }

    result.title       = extract_og(body, "og:title");
    result.description = extract_og(body, "og:description");

    if (result.title.empty()) {
        // Fall back to <title>
        static const std::regex title_re("<title[^>]*>([^<]{1,200})</title>",
            std::regex::icase | std::regex::optimize);
        std::smatch m;
        if (std::regex_search(body, m, title_re)) {
            result.title = m[1].str();
        }
    }

    // Truncate
    if (result.title.size() > 100)       result.title.resize(100);
    if (result.description.size() > 200) result.description.resize(200);

    result.success = !result.title.empty();
    return result;
}

std::string LinkPreviewer::extract_og(const std::string& html, const std::string& property) {
    // <meta property="og:title" content="..." />
    // Also handles name= variant
    std::string pattern =
        R"(<meta[^>]+property\s*=\s*["'])" + property +
        R"(["'][^>]+content\s*=\s*["']([^"']{1,300})["'])";
    std::regex re(pattern, std::regex::icase | std::regex::optimize);
    std::smatch m;
    if (std::regex_search(html, m, re)) return m[1].str();

    // Try reversed attribute order: content= before property=
    std::string pattern2 =
        R"(<meta[^>]+content\s*=\s*["']([^"']{1,300})["'][^>]+property\s*=\s*["'])" +
        property + R"(["'])";
    std::regex re2(pattern2, std::regex::icase | std::regex::optimize);
    if (std::regex_search(html, m, re2)) return m[1].str();

    return {};
}

bool LinkPreviewer::is_likely_image_url(const std::string& url) {
    std::string normalized = url;
    auto query_pos = normalized.find_first_of("?#");
    if (query_pos != std::string::npos) {
        normalized.resize(query_pos);
    }

    normalized = to_lower_ascii(normalized);
    return normalized.ends_with(".png") || normalized.ends_with(".jpg") ||
           normalized.ends_with(".jpeg") || normalized.ends_with(".gif") ||
           normalized.ends_with(".webp") || normalized.ends_with(".bmp");
}

std::string LinkPreviewer::render_image_preview(const std::string& url,
                                                const std::string& body,
                                                const std::string& content_type) const {
    (void)url;
    (void)content_type;
    if (!inline_images_ || body.empty()) {
        return {};
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(body.data()),
        static_cast<int>(body.size()),
        &width, &height, &channels, 4);
    if (!pixels || width <= 0 || height <= 0) {
        if (pixels) {
            stbi_image_free(pixels);
        }
        return {};
    }

    const bool utf8_blocks = supports_utf8_blocks();
    const int out_cols = std::max(8, image_columns_);
    const int max_rows = std::max(4, image_rows_);
    const float aspect = static_cast<float>(height) / static_cast<float>(width);
    const int out_rows = std::clamp(
        static_cast<int>(std::round(aspect * static_cast<float>(out_cols) * 0.5f)),
        4, max_rows);

    std::string rendered;
    for (int y = 0; y < out_rows; ++y) {
        if (!rendered.empty()) {
            rendered += '\n';
        }

        int src_y = std::min(height - 1, y * height / out_rows);
        for (int x = 0; x < out_cols; ++x) {
            int src_x = std::min(width - 1, x * width / out_cols);
            size_t idx = static_cast<size_t>((src_y * width + src_x) * 4);
            float alpha = static_cast<float>(pixels[idx + 3]) / 255.0f;
            float r = static_cast<float>(pixels[idx + 0]) * alpha;
            float g = static_cast<float>(pixels[idx + 1]) * alpha;
            float b = static_cast<float>(pixels[idx + 2]) * alpha;
            float luminance = (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.0f;
            rendered += intensity_to_symbol(luminance, utf8_blocks);
        }
    }

    stbi_image_free(pixels);
    return rendered;
}

} // namespace grotto
