#include "grotto/tui/admin_protocol.hpp"

#include <boost/endian/conversion.hpp>

namespace grotto::tui::protocol {

std::vector<uint8_t> encode_frame(const nlohmann::json& msg)
{
    const std::string payload = msg.dump();
    const auto payload_size = static_cast<uint32_t>(payload.size());
    const uint32_t be_len = boost::endian::native_to_big(payload_size);

    std::vector<uint8_t> frame(4 + payload.size());
    std::memcpy(frame.data(), &be_len, 4);
    std::memcpy(frame.data() + 4, payload.data(), payload.size());
    return frame;
}

uint32_t decode_frame_length(const uint8_t* data, size_t len)
{
    if (len < 4) return 0;
    uint32_t be_len = 0;
    std::memcpy(&be_len, data, 4);
    return boost::endian::big_to_native(be_len);
}

nlohmann::json to_json_event(const std::string& type, const nlohmann::json& data)
{
    return nlohmann::json{{"type", type}, {"data", data}};
}

nlohmann::json to_json_command(const std::string& cmd, const nlohmann::json& params)
{
    return nlohmann::json{{"cmd", cmd}, {"params", params}};
}

} // namespace grotto::tui::protocol
