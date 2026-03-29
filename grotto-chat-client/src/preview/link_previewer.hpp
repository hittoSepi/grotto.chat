#pragma once
#include "db/local_store.hpp"
#include <curl/curl.h>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <queue>
#include <condition_variable>

namespace grotto {

struct PreviewResult {
    std::string url;
    std::string title;
    std::string description;
    std::string image_preview;
    bool        is_image = false;
    bool        success = false;
};

using PreviewCallback = std::function<void(PreviewResult)>;

// Asynchronous link previewer. Fetches OG metadata via libcurl.
// Callbacks are invoked on the preview thread; callers must post_ui() themselves.
class LinkPreviewer {
public:
    explicit LinkPreviewer(db::LocalStore& store, int fetch_timeout_s = 5,
                           int max_cache = 200,
                           bool inline_images = true,
                           int image_columns = 40,
                           int image_rows = 16);
    ~LinkPreviewer();

    // Enqueue a URL for fetching. Callback is called when done (or on error).
    void fetch(const std::string& url, PreviewCallback callback);

    // Start the background fetch thread.
    void start();

    // Stop and join the background thread.
    void stop();

private:
    struct FetchJob {
        std::string     url;
        PreviewCallback callback;
    };

    void worker_loop();
    PreviewResult fetch_sync(const std::string& url);

    // OG tag extraction
    static std::string extract_og(const std::string& html, const std::string& property);
    static bool is_likely_image_url(const std::string& url);
    std::string render_image_preview(const std::string& url,
                                     const std::string& body,
                                     const std::string& content_type) const;

    // libcurl write callback
    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);

    db::LocalStore&    store_;
    int                fetch_timeout_s_;
    int                max_cache_;
    bool               inline_images_;
    int                image_columns_;
    int                image_rows_;

    std::thread              thread_;
    std::mutex               mu_;
    std::condition_variable  cv_;
    std::queue<FetchJob>     queue_;
    bool                     stopping_ = false;

    static constexpr size_t kMaxBodyBytes = 4 * 1024 * 1024;  // 4 MB
    static constexpr int    kMaxRedirects = 3;
};

} // namespace grotto
