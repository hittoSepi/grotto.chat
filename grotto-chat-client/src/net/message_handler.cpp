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
    case MT_TYPING:         handle_typing(env);         break;
    case MT_READ_RECEIPT:   handle_read_receipt(env);   break;
    case MT_KEY_BUNDLE:     handle_key_bundle(env);     break;
    case MT_IDENTITY_RESET: handle_identity_reset(env); break;
    case MT_PRESENCE:       handle_presence(env);       break;
    case MT_VOICE_SIGNAL:      handle_voice_signal(env);      break;
    case MT_VOICE_ROOM_JOIN:   handle_voice_room_join(env);   break;
    case MT_VOICE_ROOM_LEAVE:  handle_voice_room_leave(env);  break;
    case MT_VOICE_ROOM_STATE:  handle_voice_room_state(env);  break;
    case MT_VOICE_ICE_CONFIG:  handle_voice_ice_config(env);  break;
    case MT_PING:              handle_ping(env);              break;
    case MT_ERROR:          handle_error(env);          break;
    case MT_COMMAND_RESPONSE: handle_command_response(env); break;
    case MT_FILE_CHUNK:      handle_file_chunk(env);      break;
    case MT_FILE_PROGRESS:   handle_file_progress(env);   break;
    case MT_FILE_COMPLETE:   handle_file_complete(env);   break;
    case MT_FILE_ERROR:      handle_file_error(env);      break;
    case MT_FILE_POLICY:     handle_file_policy(env);     break;
    case MT_FILE_LIST_RESPONSE: handle_file_list_response(env); break;
    case MT_FILE_CHANGED:    handle_file_changed(env);    break;
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
    {
        std::lock_guard lk(pending_sends_mu_);
        pending_sends_.clear();
        pending_repair_requests_.clear();
    }
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
            const bool is_direct_message =
                chat.recipient_id().empty() || chat.recipient_id().front() != '#';
            if (is_direct_message && !chat.sender_id().empty() &&
                chat.sender_id() != cfg_.identity.user_id) {
                const std::string dm_channel_id =
                    (chat.recipient_id() == cfg_.identity.user_id) ? chat.sender_id() : chat.recipient_id();
                if (request_key_bundle_only(chat.sender_id()) && !dm_channel_id.empty()) {
                    push_system_to_channel(
                        dm_channel_id,
                        i18n::tr(i18n::I18nKey::REPAIRING_SECURE_SESSION, chat.sender_id()));
                }
                spdlog::warn("Suppressed transient DM decrypt failure from '{}' while session repair is in progress",
                             chat.sender_id());
                return;
            }
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
    msg.message_id   = chat.message_id();
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
    // For DMs: keep the user list pinned to exactly local user + peer.
    else if (!channel_id.empty() && channel_id[0] != '#') {
        state_.set_direct_message_users(channel_id, cfg_.identity.user_id, channel_id);
    }

    const bool has_dm_candidate =
        !channel_id.empty() &&
        channel_id.front() != '#' &&
        chat.sender_id() != cfg_.identity.user_id &&
        !chat.message_id().empty();
    const Message dm_candidate = has_dm_candidate ? msg : Message{};

    state_.post_ui([this, channel_id, m = std::move(msg)]() mutable {
        state_.push_message(channel_id, std::move(m));
    });

    if (dm_read_candidate_fn_ && has_dm_candidate) {
        dm_read_candidate_fn_(channel_id, dm_candidate);
    }

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
    std::vector<PendingSend> queued;
    {
        std::lock_guard lk(pending_sends_mu_);
        pending_repair_requests_.erase(recipient_id);
        auto it = pending_sends_.find(recipient_id);
        if (it == pending_sends_.end()) {
            return;
        }
        queued = std::move(it->second);
        pending_sends_.erase(it);
    }

    int sent = 0, failed = 0;
    for (const auto& pending : queued) {
        ChatEnvelope chat = crypto_.encrypt(
            cfg_.identity.user_id, recipient_id, pending.plaintext,
            [](const std::string&) { /* session exists now — no re-request needed */ });

        if (!chat.sender_id().empty()) {
            if (!pending.message_id.empty()) {
                chat.set_message_id(pending.message_id);
            }
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
                                 const std::string& plaintext,
                                 const std::string& message_id) {
    bool first_request = false;
    {
        std::lock_guard lk(pending_sends_mu_);
        first_request = pending_sends_.find(recipient_id) == pending_sends_.end();
        pending_sends_[recipient_id].push_back(PendingSend{plaintext, message_id});
    }

    if (first_request) {
        // Only send KEY_REQUEST once per recipient (subsequent messages just queue)
        KeyRequest kr;
        kr.set_user_id(recipient_id);
        send_envelope(MT_KEY_REQUEST, kr);
        spdlog::debug("KEY_REQUEST sent for '{}', plaintext queued", recipient_id);

        // Timeout: if KEY_BUNDLE doesn't arrive in 10s, notify user and clean up
        std::thread([this, rid = recipient_id]() {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            int count = 0;
            {
                std::lock_guard lk(pending_sends_mu_);
                auto it = pending_sends_.find(rid);
                if (it == pending_sends_.end()) {
                    return;
                }
                count = static_cast<int>(it->second.size());
                pending_sends_.erase(it);
            }
            if (count > 0) {
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
    case PresenceUpdate::DND:     status = PresenceStatus::Dnd;     break;
    default:                       status = PresenceStatus::Offline; break;
    }

    const std::string uid = update.user_id();
    const std::string status_text = update.status_text();
    const int64_t status_since_ms = update.status_since_ms();
    state_.post_ui([this, uid, status, status_text, status_since_ms]() {
        state_.set_presence(uid, status, status_text, status_since_ms);
        std::string text;
        switch (status) {
        case PresenceStatus::Online:
            text = uid + " is now online";
            break;
        case PresenceStatus::Away:
            text = uid + " is away";
            break;
        case PresenceStatus::Dnd:
            text = uid + " is in do not disturb";
            break;
        default:
            text = uid + " went offline";
            break;
        }
        if ((status == PresenceStatus::Away || status == PresenceStatus::Dnd) && !status_text.empty()) {
            text += ": " + status_text;
        }
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
        std::vector<std::string> participants;
        VoiceState vs = state_.voice_snapshot();
        vs.in_voice = true;
        vs.active_channel = state.channel_id();
        vs.participants.clear();
        for (const auto& p : state.participants()) {
            vs.participants.push_back(p);
            participants.push_back(p);
        }
        state_.set_voice_state(vs);
        state_.set_voice_room_users(state.channel_id(), participants);
    });
}

void MessageHandler::handle_identity_reset(const Envelope& env) {
    IdentityReset reset;
    if (!reset.ParseFromString(env.payload()) || reset.user_id().empty()) {
        spdlog::warn("Failed to parse IdentityReset");
        return;
    }

    if (reset.user_id() == cfg_.identity.user_id) {
        return;
    }

    crypto_.reset_dm_session(reset.user_id());
    crypto_.forget_peer_identity(reset.user_id());
    {
        std::lock_guard lk(pending_sends_mu_);
        pending_sends_.erase(reset.user_id());
        pending_repair_requests_.erase(reset.user_id());
    }
    spdlog::info("Received identity reset for '{}'; cleared local DM session and peer identity",
                 reset.user_id());
}

void MessageHandler::handle_typing(const Envelope& env) {
    TypingUpdate typing;
    if (!typing.ParseFromString(env.payload())) {
        spdlog::warn("Failed to parse TypingUpdate");
        return;
    }
    if (typing_fn_) {
        typing_fn_(typing);
    }
}

bool MessageHandler::request_key_bundle_only(const std::string& recipient_id) {
    bool should_request = false;
    {
        std::lock_guard lk(pending_sends_mu_);
        if (pending_sends_.find(recipient_id) == pending_sends_.end() &&
            pending_repair_requests_.insert(recipient_id).second) {
            should_request = true;
        }
    }

    if (!should_request) {
        return false;
    }

    KeyRequest kr;
    kr.set_user_id(recipient_id);
    send_envelope(MT_KEY_REQUEST, kr);
    spdlog::info("Requested fresh KEY_BUNDLE for '{}' after DM decryption failure", recipient_id);

    std::thread([this, rid = recipient_id]() {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::lock_guard lk(pending_sends_mu_);
        pending_repair_requests_.erase(rid);
    }).detach();
    return true;
}

void MessageHandler::handle_read_receipt(const Envelope& env) {
    ReadReceipt receipt;
    if (!receipt.ParseFromString(env.payload())) {
        spdlog::warn("Failed to parse ReadReceipt");
        return;
    }
    if (read_receipt_fn_) {
        read_receipt_fn_(receipt);
    }
}

void MessageHandler::request_file_list(const std::string& recipient_id,
                                       const std::string& channel_id,
                                       uint32_t limit) {
    FileListRequest req;
    req.set_recipient_id(recipient_id);
    req.set_channel_id(channel_id);
    req.set_limit(limit);
    send_envelope(MT_FILE_LIST_REQUEST, req);
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
        state_.add_voice_room_user(join.channel_id(), join.user_id());

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
        state_.remove_voice_room_user(leave.channel_id(), leave.user_id());

        Message msg;
        msg.type = Message::Type::System;
        msg.sender_id = "system";
        msg.content = leave.user_id() + " left voice";
        msg.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state_.push_message(leave.channel_id(), std::move(msg));
    });
}

void MessageHandler::handle_voice_ice_config(const Envelope& env) {
    VoiceIceConfig cfg;
    if (!cfg.ParseFromArray(env.payload().data(),
            static_cast<int>(env.payload().size()))) {
        spdlog::warn("Failed to parse VoiceIceConfig");
        return;
    }

    RuntimeVoiceIceConfig runtime_cfg;
    runtime_cfg.from_server = true;
    runtime_cfg.turn_username = cfg.turn_username();
    runtime_cfg.turn_password = cfg.turn_password();
    runtime_cfg.ice_servers.reserve(static_cast<size_t>(cfg.ice_servers_size()));
    for (const auto& ice_server : cfg.ice_servers()) {
        if (!ice_server.empty()) {
            runtime_cfg.ice_servers.push_back(ice_server);
        }
    }

    state_.post_ui([this, runtime_cfg = std::move(runtime_cfg)]() mutable {
        state_.set_runtime_voice_ice_config(std::move(runtime_cfg));
    });

    spdlog::info("Received VoiceIceConfig from server ({} ICE servers)",
                 cfg.ice_servers_size());
    for (const auto& ice_server : cfg.ice_servers()) {
        spdlog::debug("VoiceIceConfig server entry: {}", ice_server);
    }
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

void MessageHandler::push_system_to_channel(const std::string& channel_id, const std::string& text) {
    Message msg;
    msg.type         = Message::Type::System;
    msg.content      = text;
    msg.sender_id    = "system";
    msg.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    state_.post_ui([this, channel_id, m = std::move(msg)]() mutable {
        state_.ensure_channel(channel_id);
        state_.push_message(channel_id, std::move(m));
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
        std::string detail = complete.file_id();
        if (auto info = file_mgr_->get_transfer_by_file_id(complete.file_id())) {
            if (info->direction == client::file::TransferDirection::DOWNLOAD) {
                detail = info->filename + " -> " + info->local_path.string();
            } else {
                detail = info->filename + " (" + complete.file_id() + ")";
            }
        }
        push_system(i18n::tr(i18n::I18nKey::FILE_TRANSFER_COMPLETED, detail));
    }
}

void MessageHandler::handle_file_error(const Envelope& env) {
    ::FileError error;
    if (!error.ParseFromString(env.payload())) return;
    if (file_mgr_) {
        bool already_reported = false;
        bool handled = false;
        std::string detail = error.error_message();
        if (auto info = file_mgr_->get_transfer_by_file_id(error.file_id())) {
            already_reported =
                info->state == client::file::TransferState::FAILED &&
                info->error_message == error.error_message();
            detail = info->filename + ": " + error.error_message();
        }
        file_mgr_->on_file_error(error);
        if (file_error_fn_) {
            handled = file_error_fn_(error);
        }
        if (!already_reported && !handled) {
            push_system(i18n::tr(i18n::I18nKey::FILE_TRANSFER_ERROR, detail));
        }
    }
}

void MessageHandler::handle_file_policy(const Envelope& env) {
    FileTransferPolicy policy;
    if (!policy.ParseFromString(env.payload())) {
        spdlog::warn("Failed to parse FileTransferPolicy");
        return;
    }
    if (file_policy_fn_) {
        file_policy_fn_(policy);
    }
}

void MessageHandler::handle_file_list_response(const Envelope& env) {
    FileListResponse response;
    if (!response.ParseFromString(env.payload())) {
        spdlog::warn("Failed to parse FileListResponse");
        return;
    }
    if (file_list_fn_) {
        file_list_fn_(response);
    }
}

void MessageHandler::handle_file_changed(const Envelope& env) {
    FileChanged changed;
    if (!changed.ParseFromString(env.payload())) {
        spdlog::warn("Failed to parse FileChanged");
        return;
    }
    if (file_changed_fn_) {
        file_changed_fn_(changed);
    }
}

} // namespace grotto::net
