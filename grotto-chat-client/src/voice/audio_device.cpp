// Define MA_IMPLEMENTATION in exactly one TU
#define MA_IMPLEMENTATION
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "voice/audio_device.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cctype>
#include <new>

namespace grotto::voice {

namespace {

const char* backend_name(ma_backend backend) {
    switch (backend) {
    case ma_backend_wasapi: return "wasapi";
    case ma_backend_dsound: return "dsound";
    case ma_backend_winmm: return "winmm";
    case ma_backend_coreaudio: return "coreaudio";
    case ma_backend_sndio: return "sndio";
    case ma_backend_audio4: return "audio4";
    case ma_backend_oss: return "oss";
    case ma_backend_pulseaudio: return "pulseaudio";
    case ma_backend_alsa: return "alsa";
    case ma_backend_jack: return "jack";
    case ma_backend_aaudio: return "aaudio";
    case ma_backend_opensl: return "opensl";
    case ma_backend_webaudio: return "webaudio";
    case ma_backend_null: return "null";
    default: return "unknown";
    }
}

std::string lower_copy(const std::string& value) {
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return normalized;
}

bool resolve_named_device(
    ma_context* context,
    bool capture,
    const std::string& requested_name,
    ma_device_id& resolved_id,
    std::string& resolved_name) {
    if (context == nullptr || requested_name.empty()) {
        return false;
    }

    ma_device_info* playback_infos = nullptr;
    ma_uint32 playback_count = 0;
    ma_device_info* capture_infos = nullptr;
    ma_uint32 capture_count = 0;
    if (ma_context_get_devices(
            context,
            &playback_infos,
            &playback_count,
            &capture_infos,
            &capture_count) != MA_SUCCESS) {
        return false;
    }

    auto* infos = capture ? capture_infos : playback_infos;
    const auto count = capture ? capture_count : playback_count;
    if (infos == nullptr || count == 0) {
        return false;
    }

    for (ma_uint32 i = 0; i < count; ++i) {
        if (requested_name == infos[i].name) {
            resolved_id = infos[i].id;
            resolved_name = infos[i].name;
            return true;
        }
    }

    const auto requested_lower = lower_copy(requested_name);
    for (ma_uint32 i = 0; i < count; ++i) {
        if (requested_lower == lower_copy(infos[i].name)) {
            resolved_id = infos[i].id;
            resolved_name = infos[i].name;
            return true;
        }
    }

    return false;
}

} // namespace

// ── Callback ─────────────────────────────────────────────────────────────────

void AudioDevice::data_callback(ma_device* dev, void* out, const void* in, uint32_t frames) {
    auto* self = static_cast<AudioDevice*>(dev->pUserData);
    if (!self) return;

    if (in && self->capture_cb_) {
        self->capture_cb_(static_cast<const float*>(in), frames);
    }
    if (out && self->playback_cb_) {
        self->playback_cb_(static_cast<float*>(out), frames);
    }
}

// ── AudioDevice ───────────────────────────────────────────────────────────────

AudioDevice::AudioDevice()
    : device_(new (std::nothrow) ma_device{}),
      context_(new (std::nothrow) ma_context{}) {}

AudioDevice::~AudioDevice() {
    close();
    delete device_;
    delete context_;
}

bool AudioDevice::open(const std::string& input_device,
                        const std::string& output_device,
                        CaptureCallback  on_capture,
                        PlaybackCallback on_playback) {
    if (open_) close();

    capture_cb_  = std::move(on_capture);
    playback_cb_ = std::move(on_playback);

    ma_device_config config = ma_device_config_init(ma_device_type_duplex);
    config.sampleRate         = 48000;
    config.capture.format     = ma_format_f32;
    config.capture.channels   = 1;
    config.playback.format    = ma_format_f32;
    config.playback.channels  = 1;
    config.periodSizeInMilliseconds = 5;  // 5ms periods
    config.dataCallback       = data_callback;
    config.pUserData          = this;

    spdlog::info("Opening audio device (requested_input='{}', requested_output='{}', sample_rate=48000, channels=1, period_ms=5)",
                 input_device.empty() ? "<system-default>" : input_device,
                 output_device.empty() ? "<system-default>" : output_device);

    if (device_ == nullptr || context_ == nullptr) {
        spdlog::error("Audio device allocation failed");
        return false;
    }

    ma_result context_result = ma_context_init(nullptr, 0, nullptr, context_);
    if (context_result != MA_SUCCESS) {
        spdlog::error("ma_context_init failed: {}", static_cast<int>(context_result));
        return false;
    }

    ma_device_id input_id{};
    ma_device_id output_id{};
    std::string resolved_input_name;
    std::string resolved_output_name;

    if (!input_device.empty()) {
        if (resolve_named_device(context_, true, input_device, input_id, resolved_input_name)) {
            config.capture.pDeviceID = &input_id;
            spdlog::info("Resolved input device '{}' -> '{}'", input_device, resolved_input_name);
        } else {
            spdlog::warn("Input device '{}' was not found; using system default instead", input_device);
        }
    }

    if (!output_device.empty()) {
        if (resolve_named_device(context_, false, output_device, output_id, resolved_output_name)) {
            config.playback.pDeviceID = &output_id;
            spdlog::info("Resolved output device '{}' -> '{}'", output_device, resolved_output_name);
        } else {
            spdlog::warn("Output device '{}' was not found; using system default instead", output_device);
        }
    }

    ma_result result = ma_device_init(context_, &config, device_);
    if (result != MA_SUCCESS) {
        spdlog::error("ma_device_init failed: {}", static_cast<int>(result));
        ma_context_uninit(context_);
        return false;
    }

    open_ = true;
    spdlog::info("Audio device opened (capture='{}', playback='{}', 48kHz, mono, duplex)",
                 device_->capture.name[0] != '\0' ? device_->capture.name : "<unknown>",
                 device_->playback.name[0] != '\0' ? device_->playback.name : "<unknown>");
    spdlog::info("Audio backend: {}",
                 (device_->pContext != nullptr) ? backend_name(device_->pContext->backend) : "unknown");
    return true;
}

void AudioDevice::start() {
    if (!open_ || started_) return;
    ma_result r = ma_device_start(device_);
    if (r != MA_SUCCESS) {
        spdlog::error("ma_device_start failed: {}", static_cast<int>(r));
        return;
    }
    started_ = true;
}

void AudioDevice::stop() {
    if (!started_) return;
    ma_device_stop(device_);
    started_ = false;
}

void AudioDevice::close() {
    stop();
    if (open_) {
        ma_device_uninit(device_);
        ma_context_uninit(context_);
        open_ = false;
    }
}

std::vector<std::string> AudioDevice::list_input_devices() {
    std::vector<std::string> names;
    ma_context ctx{};
    if (ma_context_init(nullptr, 0, nullptr, &ctx) != MA_SUCCESS) return names;

    ma_device_info* infos = nullptr;
    ma_uint32       count = 0;
    if (ma_context_get_devices(&ctx, nullptr, nullptr, &infos, &count) == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < count; ++i) {
            names.emplace_back(infos[i].name);
        }
    }
    ma_context_uninit(&ctx);
    return names;
}

std::vector<std::string> AudioDevice::list_output_devices() {
    std::vector<std::string> names;
    ma_context ctx{};
    if (ma_context_init(nullptr, 0, nullptr, &ctx) != MA_SUCCESS) return names;

    ma_device_info* infos = nullptr;
    ma_uint32       count = 0;
    if (ma_context_get_devices(&ctx, &infos, &count, nullptr, nullptr) == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < count; ++i) {
            names.emplace_back(infos[i].name);
        }
    }
    ma_context_uninit(&ctx);
    return names;
}

} // namespace grotto::voice
