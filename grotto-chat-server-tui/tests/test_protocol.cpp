#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "grotto/tui/admin_protocol.hpp"

namespace proto = grotto::tui::protocol;

void test_frame_roundtrip()
{
    nlohmann::json original = {{"hello", "world"}, {"num", 42}};
    auto frame = proto::encode_frame(original);

    // Must have at least 4 bytes for length prefix
    assert(frame.size() > 4);

    // Decode length
    uint32_t payload_len = proto::decode_frame_length(frame.data(), frame.size());
    assert(payload_len == frame.size() - 4);

    // Parse payload
    std::string payload_str(frame.begin() + 4, frame.end());
    nlohmann::json decoded = nlohmann::json::parse(payload_str);
    assert(decoded == original);

    std::cout << "  PASS: test_frame_roundtrip\n";
}

void test_decode_frame_length_short_buffer()
{
    uint8_t buf[2] = {0x00, 0x01};
    assert(proto::decode_frame_length(buf, 2) == 0);

    std::cout << "  PASS: test_decode_frame_length_short_buffer\n";
}

void test_log_entry_serialization()
{
    proto::LogEntry entry;
    entry.level = "info";
    entry.msg = "Server started";
    entry.ts = "2026-03-15T12:00:00Z";

    nlohmann::json j = entry;

    assert(j["level"] == "info");
    assert(j["msg"] == "Server started");
    assert(j["ts"] == "2026-03-15T12:00:00Z");

    // Round-trip
    auto back = j.get<proto::LogEntry>();
    assert(back.level == entry.level);
    assert(back.msg == entry.msg);
    assert(back.ts == entry.ts);

    std::cout << "  PASS: test_log_entry_serialization\n";
}

void test_kick_command_serialization()
{
    proto::KickCommand kick;
    kick.user_id = "user123";
    kick.reason = "spamming";

    nlohmann::json params = kick;
    auto envelope = proto::to_json_command("kick", params);

    assert(envelope["cmd"] == "kick");
    assert(envelope["params"]["user_id"] == "user123");
    assert(envelope["params"]["reason"] == "spamming");

    // Deserialize params back
    auto back = envelope["params"].get<proto::KickCommand>();
    assert(back.user_id == kick.user_id);
    assert(back.reason == kick.reason);

    std::cout << "  PASS: test_kick_command_serialization\n";
}

void test_user_info_list()
{
    std::vector<proto::UserInfo> users;
    users.push_back({"alice", "10.0.0.1", "Alice", "2026-03-15T10:00:00Z"});
    users.push_back({"bob", "10.0.0.2", "Bob", "2026-03-15T11:00:00Z"});

    nlohmann::json data = users;
    auto envelope = proto::to_json_event("users", data);

    assert(envelope["type"] == "users");
    assert(envelope["data"].is_array());
    assert(envelope["data"].size() == 2);
    assert(envelope["data"][0]["id"] == "alice");
    assert(envelope["data"][1]["nickname"] == "Bob");

    // Deserialize back
    auto back = envelope["data"].get<std::vector<proto::UserInfo>>();
    assert(back.size() == 2);
    assert(back[0].ip == "10.0.0.1");
    assert(back[1].connected == "2026-03-15T11:00:00Z");

    std::cout << "  PASS: test_user_info_list\n";
}

int main()
{
    std::cout << "admin_protocol tests:\n";
    test_frame_roundtrip();
    test_decode_frame_length_short_buffer();
    test_log_entry_serialization();
    test_kick_command_serialization();
    test_user_info_list();
    std::cout << "All tests passed.\n";
    return 0;
}
