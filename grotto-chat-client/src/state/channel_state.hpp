#pragma once
#include <deque>
#include <optional>
#include <string>
#include <cstdint>
#include <vector>

namespace grotto {

struct InlineImageThumbnail {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
};

struct Message {
    std::string sender_id;
    std::string content;
    int64_t     timestamp_ms = 0;

    enum class Type { Chat, System, VoiceEvent, Preview } type = Type::Chat;

    // Pending link preview (filled asynchronously)
    struct Preview {
        std::string title;
        std::string description;
        bool        loaded = false;
    };
    std::optional<Preview> link_preview;
    std::optional<InlineImageThumbnail> inline_image;
};

struct ChannelState {
    std::deque<Message> messages;    // max 1000, old ones dropped
    int unread_count  = 0;
    int scroll_offset = 0;           // lines scrolled up from bottom
};

} // namespace grotto
