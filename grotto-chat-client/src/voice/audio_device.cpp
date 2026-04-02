// Define MA_IMPLEMENTATION in exactly one TU
#define MA_IMPLEMENTATION
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "voice/audio_device.hpp"
#include <spdlog/spdlog.h>
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
    : device_(new (std::nothrow) ma_device{}) {}

AudioDevice::~AudioDevice() {
    close();
    delete device_;
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

    // Device selection (empty = default)
    // For named device selection, miniaudio requires enumeration first.
    // For now, always use system default; named device support can be added.
    if (!input_device.empty() || !output_device.empty()) {
        spdlog::warn("Named audio device selection is not implemented yet; using system defaults instead");
    }
    (void)input_device;
    (void)output_device;

    ma_result result = ma_device_init(nullptr, &config, device_);
    if (result != MA_SUCCESS) {
        spdlog::error("ma_device_init failed: {}", static_cast<int>(result));
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
