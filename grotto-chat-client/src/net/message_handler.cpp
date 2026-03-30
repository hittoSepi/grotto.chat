#include "net/message_handler.hpp"
#include "file/file_transfer.hpp"
#include "net/net_client.hpp"
#include "crypto/crypto_engine.hpp"
#include "voice/voice_engine.hpp"
#include "version.hpp"
#include "i18n/strings.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <algorithm>

namespace grotto::net {

namespace {

std::string recipient_from_not_found_error(const std::string& message) {
    static constexpr std::string_view kPrefix = "User not found: ";
    if (message.rfind(kPrefix.data(), 0) == 0) {
        return message.substr(kPrefix.size());
    }
    return {};
}

}

MessageHandler::MessageHandler(AppState& state,
                                crypto::CryptoEngine& crypto,
                                const ClientConfig& cfg)
    : state_(state), crypto_(crypto), cfg_(cfg) {}

void MessageHandler::dispatch(const Envelope& env) {
    switch (env.type()) {
    case MT_AUTH_CHALLENGE: handle_auth_challenge(env); break;
    case MT_AUTH_OK:        handle_auth_ok(env);        break;
    case MT_AUTH_FAIL:      handle_auth_fail(env);      break;
    case MT_CHAT_ENVELOPE:  handle_chat(env);           break;
    case MT_KEY_BUNDLE:     handle_key_bundle(env);     break;
    case MT_PRESENCE:       handle_presence(env);       break;
    case MT_VOICE_SIGNAL:      handle_voice_signal(env);      break;
    case MT_VOICE_ROOM_JOIN:   handle_voice_room_join(env);   break;
    case MT_VOICE_ROOM_LEAVE:  handle_voice_room_leave(env);  break;
    case MT_VOICE_ROOM_STATE:  handle_voice_room_state(env);  break;
    case MT_PING:              handle_ping(env);              break;
    case MT_ERROR:          handle_error(env);          break;
    case MT_COMMAND_RESPONSE: handle_command_response(env); break;
    case MT_FILE_CHUNK:      handle_file_chunk(env);      break;
    case MT_FILE_PROGRESS:   handle_file_progress(env);   break;
    case MT_FILE_COMPLETE:   handle_file_complete(env);   break;
    case MT_FILE_ERROR:      handle_file_error(env);      break;
    default:
        spdlog::debug("Unhandled message type: {}", static_cast<int>(env.type()));
        break;
    }
}

void MessageHandler::handle_auth_challenge(const Envelope& env) {
    AuthChallenge challenge;
    if (!challenge.ParseFromString(env.payload())) {
        spdlog::error("Failed to parse AuthChallenge");
        return;
    }

    const std::string& nonce_str = challenge.nonce();
    std::vector<uint8_t> nonce(nonce_str.begin(), nonce_str.end());

    spdlog::debug("Received auth challenge, signing...");
    if (trace_fn_) trace_fn_("Auth challenge received");

    auto sig     = crypto_.sign_challenge(nonce, cfg_.identity.user_id);
    auto id_pub  = crypto_.identity_pub();
    auto spk_inf = crypto_.current_spk();

    AuthResponse resp;
    resp.set_user_id(cfg_.identity.user_id);
    resp.set_identity_pub(id_pub.data(), id_pub.size());
    resp.set_signature(sig.data(), sig.size());
    resp.set_signed_prekey(spk_inf.pub.data(), spk_inf.pub.size());
    resp.set_spk_sig(spk_inf.sig.data(), spk_inf.sig.size());
    resp.set_spk_id(spk_inf.id);
    // Include password for key recovery if configured
    if (!cfg_.identity.password.empty()) {
        resp.set_password(cfg_.identity.password);
    }

    send_envelope(MT_AUTH_RESPONSE, resp);
    if (trace_fn_) trace_fn_("Auth response sent");
}

void MessageHandler::handle_auth_ok(const Envelope& env) {
    (void)env;
    authenticated_ = true;
    if (trace_fn_) trace_fn_("Authenticated");
    spdlog::info("Authentication successful");
    push_system(i18n::tr(i18n::I18nKey::AUTHENTICATED_AS, cfg_.identity.user_id));
    state_.set_connected(true);
    state_.set_connecting(false);
    on_auth_ok();
}

void MessageHandler::on_auth_ok() {
    // Upload pre-keys
    auto ku = crypto_.prepare_key_upload(100);
    send_envelope(MT_KEY_UPLOAD, ku);
    spdlog::debug("Uploaded {} one-time pre-keys", ku.opk_ids_size());

    if (onboarding_shown_) {
        return;
    }
    onboarding_shown_ = true;

    if (cfg_.ui.language == "fi") {
        push_system("Aloitus: liity kanavalle komennolla /join #<kanava>");
        push_system("Pika-apu: /help tai /help commands");
        push_system("Yksityisviesti: /msg <nimi> <teksti> (alias /w)");
        push_system("Asetukset: F12");
    } else {
        push_system("Getting started: join a channel with /join #<channel>");
        push_system("Quick help: /help or /help commands");
        push_system("Private message: /msg <name> <text> (alias /w)");
        push_system("Settings: F12");
    }
}

void MessageHandler::on_transport_disconnected() {
    authenticated_ = false;
    state_.set_connecting(false);
    pending_sends_.clear();
    crypto_.reset_all_dm_sessions();
}

void MessageHandler::handle_auth_fail(const Envelope& env) {
    authenticated_ = false;
    if (trace_fn_) trace_fn_("AUTH_FAIL received");

    Error err;
    std::string detail;
    if (err.ParseFromString(env.payload())) {
        detail = err.message();
        spdlog::error("Authentication failed (code {}): {}", err.code(), detail);
    } else {
        spdlog::error("Authentication failed");
    }

    if (!detail.empty()) {
        push_system(i18n::tr(i18n::I18nKey::AUTH_FAILED, detail));
    } else {
        push_system(i18n::tr(i18n::I18nKey::AUTH_FAILED_CHECK_KEY));
    }
    state_.set_connected(false);
    state_.set_connecting(false);
}

void MessageHandler::handle_chat(const Envelope& env) {
    ChatEnvelope chat;
    if (!chat.ParseFromString(env.payload())) {
        spdlog::warn("Failed to parse ChatEnvelope");
        return;
    }

    std::string plaintext;
    bool        decrypted = false;

    if (chat.ciphertext_type() == 0) {
        // Unencrypted (should not happen in production)
        plaintext = chat.ciphertext();
        decrypted = true;
    } else {
        auto result = crypto_.decrypt(chat);
        if (result.success) {
            plaintext = result.plaintext;
            decrypted = true;
        } else {
            plaintext = "[decryption failed]";
            decrypted = true;
        }
    }

    if (!decrypted) return;

    // Determine channel_id: for DMs, use the OTHER person's name
    std::string channel_id;
    if (!chat.recipient_id().empty() && chat.recipient_id()[0] == '#') {
        // Channel message: use recipient_id (the channel name)
        channel_id = chat.recipient_id();
    } else if (chat.recipient_id() == cfg_.identity.user_id) {
        // DM received by me: use sender_id (the other person)
        channel_id = chat.sender_id();
    } else {
        // DM sent by me: use recipient_id (the other person)
        channel_id = chat.recipient_id();
    }
    if (channel_id.empty()) channel_id = chat.sender_id();

    state_.ensure_channel(channel_id);

    Message msg;
    msg.sender_id   = chat.sender_id();
    msg.content     = plaintext;
    msg.timestamp_ms = static_cast<int64_t>(env.timestamp_ms());
    if (msg.timestamp_ms == 0) {
        msg.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    msg.type = Message::Type::Chat;

    // Persist to database if callback is set
    if (persist_fn_) {
        persist_fn_(channel_id, msg);
    }

    // Ensure sender appears in channel user list (for channel messages)
    std::string sender = chat.sender_id();
    if (!channel_id.empty() && channel_id[0] == '#' && !sender.empty()) {
        auto existing = state_.channel_user(channel_id, sender);
        if (!existing.has_value()) {
            ChannelUserInfo info;
            info.user_id = sender;
            info.role = UserRole::Regular;
            info.presence = PresenceStatus::Online;
            state_.add_channel_user(channel_id, info);
        }
    }
    // For DMs: ensure both participants appear in the user list
    else if (!channel_id.empty() && channel_id[0] != '#' && !sender.empty()) {
        // Add the sender (the other person) if not already present
        if (sender != cfg_.identity.user_id) {
            auto existing = state_.channel_user(channel_id, sender);
            if (!existing.has_value()) {
                ChannelUserInfo info;
                info.user_id = sender;
                info.role = UserRole::Regular;
                info.presence = PresenceStatus::Online;
                state_.add_channel_user(channel_id, info);
            }
        }
        // Also ensure the recipient (other person) is in the list
        std::string other = chat.recipient_id();
        if (other != cfg_.identity.user_id && !other.empty()) {
            auto existing = state_.channel_user(channel_id, other);
            if (!existing.has_value()) {
                ChannelUserInfo info;
                info.user_id = other;
                info.role = UserRole::Regular;
                info.presence = PresenceStatus::Online;
                state_.add_channel_user(channel_id, info);
            }
        }
    }

    state_.post_ui([this, channel_id, m = std::move(msg)]() mutable {
        state_.push_message(channel_id, std::move(m));
    });

    // Trigger link preview AFTER message is queued so preview appears below
    if (preview_fn_) {
        preview_fn_(channel_id, plaintext);
    }
}

void MessageHandler::handle_key_bundle(const Envelope& env) {
    spdlog::debug("Received KeyBundle payload: size={}", env.payload().size());
    
    KeyBundle bundle;
    if (!bundle.ParseFromString(env.payload())) {
        spdlog::warn("Failed to parse KeyBundle");
        return;
    }

    std::string recipient_id = bundle.recipient_for();
    spdlog::debug("Parsed KeyBundle: recipient_for='{}'", recipient_id);
    
    // WORKAROUND: If recipient_for is empty, use the pending key request target
    if (recipient_id.empty()) {
        spdlog::warn("KeyBundle missing recipient_for field — using pending request target");
        // Get the target from the pending key request queue
        // For now, use the identity key to determine the sender
        if (bundle.identity_pub().size() >= 32) {
            // We need to look up the user by identity key
            // This is a workaround - the server should send recipient_for
            spdlog::debug("Bundle has identity_pub, attempting to identify sender by key");
        }
        // Try to use the last key request target as fallback
        // This requires tracking pending key requests
        spdlog::error("Cannot determine recipient without recipient_for field — need server fix");
        return;
    }

    // Establish X3DH session with the recipient
    if (!crypto_.on_key_bundle(bundle, recipient_id)) {
        push_system(i18n::tr(i18n::I18nKey::FAILED_ESTABLISH_SESSION, recipient_id));
        return;
    }

    // Flush all pending plaintexts queued while waiting for this bundle
    auto it = pending_sends_.find(recipient_id);
    if (it == pending_sends_.end()) return;

    std::vector<std::string> queued = std::move(it->second);
    pending_sends_.erase(it);

    int sent = 0, failed = 0;
    for (const auto& plaintext : queued) {
        ChatEnvelope chat = crypto_.encrypt(
            cfg_.identity.user_id, recipient_id, plaintext,
            [](const std::string&) { /* session exists now — no re-request needed */ });

        if (!chat.sender_id().empty()) {
            send_envelope(MT_CHAT_ENVELOPE, chat);
            ++sent;
        } else {
            spdlog::error("Re-encrypt failed for pending DM to '{}'", recipient_id);
            ++failed;
        }
    }
    spdlog::debug("Flushed {} pending DM(s) to '{}' ({} failed)", sent, recipient_id, failed);
    if (failed > 0) {
        push_system(i18n::tr(i18n::I18nKey::FAILED_SEND_MESSAGES, std::to_string(failed), recipient_id));
    }
}

void MessageHandler::request_key(const std::string& recipient_id,
                                  const std::string& plaintext) {
    bool first_request = pending_sends_.find(recipient_id) == pending_sends_.end();
    pending_sends_[recipient_id].push_back(plaintext);

    if (first_request) {
        // Only send KEY_REQUEST once per recipient (subsequent messages just queue)
        KeyRequest kr;
        kr.set_user_id(recipient_id);
        send_envelope(MT_KEY_REQUEST, kr);
        spdlog::debug("KEY_REQUEST sent for '{}', plaintext queued", recipient_id);

        // Timeout: if KEY_BUNDLE doesn't arrive in 10s, notify user and clean up
        std::thread([this, rid = recipient_id]() {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            auto it = pending_sends_.find(rid);
            if (it != pending_sends_.end()) {
                int count = static_cast<int>(it->second.size());
                pending_sends_.erase(it);
                push_system(i18n::tr(i18n::I18nKey::COULD_NOT_ESTABLISH_SESSION, rid) +
                            " (timeout) — " + std::to_string(count) + " message(s) dropped");
                spdlog::warn("KEY_BUNDLE timeout for '{}', {} messages dropped", rid, count);
            }
        }).detach();
    } else {
        spdlog::debug("Additional plaintext queued for '{}' (waiting for KEY_BUNDLE)", recipient_id);
    }
}

void MessageHandler::handle_presence(const Envelope& env) {
    PresenceUpdate update;
    if (!update.ParseFromString(env.payload())) return;

    PresenceStatus status;
    switch (update.status()) {
    case PresenceUpdate::ONLINE:  status = PresenceStatus::Online;  break;
    case PresenceUpdate::AWAY:    status = PresenceStatus::Away;    break;
    default:                       status = PresenceStatus::Offline; break;
    }

    const std::string& uid = update.user_id();
    state_.post_ui([this, uid, status]() {
        state_.set_presence(uid, status);
        std::string text = uid + (status == PresenceStatus::Online ? " is now online" : " went offline");
        push_system(text);
    });
}

void MessageHandler::handle_voice_signal(const Envelope& env) {
    // Forward to VoiceEngine
    VoiceSignal vs;
    if (!vs.ParseFromString(env.payload())) {
        spdlog::warn("Failed to parse VoiceSignal");
        return;
    }

    if (voice_engine_) {
        voice_engine_->on_voice_signal(vs);
        spdlog::debug("Voice signal received from {}", vs.from_user());
    }
}

void MessageHandler::handle_voice_room_state(const Envelope& env) {
    VoiceRoomState state;
    if (!state.ParseFromArray(env.payload().data(),
            static_cast<int>(env.payload().size()))) return;

    // Tell VoiceEngine to set up peer connections to all participants
    if (voice_engine_) {
        std::vector<std::string> peers;
        for (const auto& p : state.participants()) {
            if (p != cfg_.identity.user_id) {
                peers.push_back(p);
            }
        }
        voice_engine_->on_room_joined(state.channel_id(), peers);
    }

    state_.post_ui([this, state]() {
        VoiceState vs = state_.voice_snapshot();
        vs.in_voice = true;
        vs.active_channel = state.channel_id();
        vs.participants.clear();
        for (const auto& p : state.participants()) {
            vs.participants.push_back(p);
        }
        state_.set_voice_state(vs);
    });
}

void MessageHandler::handle_voice_room_join(const Envelope& env) {
    VoiceRoomJoin join;
    if (!join.ParseFromArray(env.payload().data(),
            static_cast<int>(env.payload().size()))) return;

    // Another user joined our room — create peer connection to them
    if (voice_engine_) {
        voice_engine_->on_peer_joined(join.user_id());
    }

    state_.post_ui([this, join]() {
        VoiceState vs = state_.voice_snapshot();
        vs.participants.push_back(join.user_id());
        state_.set_voice_state(vs);

        Message msg;
        msg.type = Message::Type::System;
        msg.sender_id = "system";
        msg.content = join.user_id() + " joined voice";
        msg.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state_.push_message(join.channel_id(), std::move(msg));
    });
}

void MessageHandler::handle_voice_room_leave(const Envelope& env) {
    VoiceRoomLeave leave;
    if (!leave.ParseFromArray(env.payload().data(),
            static_cast<int>(env.payload().size()))) return;

    // Remove peer connection
    if (voice_engine_) {
        voice_engine_->on_peer_left(leave.user_id());
    }

    state_.post_ui([this, leave]() {
        VoiceState vs = state_.voice_snapshot();
        auto& p = vs.participants;
        p.erase(std::remove(p.begin(), p.end(), leave.user_id()), p.end());
        state_.set_voice_state(vs);

        Message msg;
        msg.type = Message::Type::System;
        msg.sender_id = "system";
        msg.content = leave.user_id() + " left voice";
        msg.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state_.push_message(leave.channel_id(), std::move(msg));
    });
}

void MessageHandler::handle_ping(const Envelope& env) {
    (void)env;
    Envelope pong;
    pong.set_seq(next_seq_++);
    pong.set_timestamp_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    pong.set_type(MT_PONG);
    if (net_client_) net_client_->send(pong);
}

void MessageHandler::handle_error(const Envelope& env) {
    Error err;
    if (!err.ParseFromString(env.payload())) return;
    spdlog::error("Server error {}: {}", err.code(), err.message());

    if (err.code() == 4060) {
        std::string recipient_id = recipient_from_not_found_error(err.message());
        if (recipient_id.empty() && pending_sends_.size() == 1) {
            recipient_id = pending_sends_.begin()->first;
        }
        if (!recipient_id.empty()) {
            pending_sends_.erase(recipient_id);
            push_system(i18n::tr(i18n::I18nKey::USER_NOT_FOUND, recipient_id));
            return;
        }
    }

    push_system(i18n::tr(i18n::I18nKey::SERVER_ERROR, err.message()));
}

void MessageHandler::handle_command_response(const Envelope& env) {
    CommandResponse response;
    if (!response.ParseFromString(env.payload())) {
        spdlog::warn("Failed to parse CommandResponse");
        return;
    }

    if (command_response_fn_) {
        command_response_fn_(response);
        return;
    }

    push_system(i18n::tr(i18n::I18nKey::COMMAND_RESPONSE, response.command(), response.message()));
}

void MessageHandler::send_envelope(MessageType type, const google::protobuf::Message& msg) {
    if (!net_client_) return;

    Envelope env;
    env.set_seq(next_seq_++);
    env.set_timestamp_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    env.set_type(type);
    env.set_payload(msg.SerializeAsString());

    net_client_->send(env);
}

void MessageHandler::push_system(const std::string& text) {
    Message msg;
    msg.type         = Message::Type::System;
    msg.content      = text;
    msg.sender_id    = "system";
    msg.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    state_.post_ui([this, m = std::move(msg)]() mutable {
        state_.ensure_channel("server");
        state_.push_message("server", std::move(m));
    });
}

void MessageHandler::send_hello() {
    Hello hello;
    hello.set_protocol_version(1);
    hello.set_client_version(std::string("grotto-client/") + std::string(grotto::VERSION));
    send_envelope(MT_HELLO, hello);
}

void MessageHandler::send_command(const std::string& cmd, const std::vector<std::string>& args) {
    if (!net_client_) return;
    
    IrcCommand ic;
    ic.set_command(cmd);
    for (const auto& arg : args) {
        ic.add_args(arg);
    }
    
    send_envelope(MT_COMMAND, ic);
    spdlog::debug("Sent command: {} with {} args", cmd, args.size());
}

void MessageHandler::handle_file_chunk(const Envelope& env) {
    ::FileChunk chunk;
    if (!chunk.ParseFromString(env.payload())) return;

    if (file_mgr_) {
        client::file::FileChunk fc;
        fc.file_id = chunk.file_id();
        fc.chunk_index = chunk.chunk_index();
        fc.data.assign(chunk.data().begin(), chunk.data().end());
        fc.is_last = chunk.is_last();
        file_mgr_->on_file_chunk(fc);
    }
}

void MessageHandler::handle_file_progress(const Envelope& env) {
    ::FileProgress progress;
    if (!progress.ParseFromString(env.payload())) return;
    if (file_mgr_) file_mgr_->on_file_progress(progress);
}

void MessageHandler::handle_file_complete(const Envelope& env) {
    ::FileComplete complete;
    if (!complete.ParseFromString(env.payload())) return;
    if (file_mgr_) {
        file_mgr_->on_file_complete(complete);
        push_system(i18n::tr(i18n::I18nKey::FILE_TRANSFER_COMPLETED, complete.file_id()));
    }
}

void MessageHandler::handle_file_error(const Envelope& env) {
    ::FileError error;
    if (!error.ParseFromString(env.payload())) return;
    if (file_mgr_) {
        file_mgr_->on_file_error(error);
        push_system(i18n::tr(i18n::I18nKey::FILE_TRANSFER_ERROR, error.error_message()));
    }
}

} // namespace grotto::net
