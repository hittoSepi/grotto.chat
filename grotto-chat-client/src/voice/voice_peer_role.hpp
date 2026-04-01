#pragma once

#include <string_view>

namespace grotto::voice {

inline bool should_offer_to_peer(std::string_view local_user_id, std::string_view peer_id) {
    return local_user_id < peer_id;
}

} // namespace grotto::voice
