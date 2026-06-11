#include "AudioCapture.h"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <cstring>

namespace {
constexpr size_t kRingSize = 16384; // > FFT size, ~340 ms at 48 kHz

const pw_stream_events kStreamEvents = {
    .version = PW_VERSION_STREAM_EVENTS,
    .param_changed = AudioCapture::onParamChanged,
    .process = AudioCapture::onProcess,
};
} // namespace

AudioCapture::AudioCapture() : ring_(kRingSize, 0.0f) {
    pw_init(nullptr, nullptr);
}

AudioCapture::~AudioCapture() {
    stop();
    pw_deinit();
}

bool AudioCapture::start() {
    if (running_) return true;

    loop_ = pw_thread_loop_new("sakia-audio", nullptr);
    if (!loop_) return false;

    pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_APP_NAME, "SakiaVU",
        nullptr);

    stream_ = pw_stream_new_simple(pw_thread_loop_get_loop(loop_), "SakiaVU capture",
                                   props, &kStreamEvents, this);
    if (!stream_) {
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
        return false;
    }

    uint8_t buffer[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    spa_audio_info_raw info{};
    info.format = SPA_AUDIO_FORMAT_F32;
    info.rate = 48000;
    info.channels = 1;
    const spa_pod* params[1] = {spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info)};

    int res = pw_stream_connect(stream_, PW_DIRECTION_INPUT, PW_ID_ANY,
                                static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                                             PW_STREAM_FLAG_MAP_BUFFERS),
                                params, 1);
    if (res < 0) {
        stop();
        return false;
    }

    pw_thread_loop_start(loop_);
    running_ = true;
    return true;
}

void AudioCapture::stop() {
    if (loop_) pw_thread_loop_stop(loop_);
    if (stream_) {
        pw_stream_destroy(stream_);
        stream_ = nullptr;
    }
    if (loop_) {
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
    }
    running_ = false;

    std::lock_guard<std::mutex> lock(mutex_);
    std::fill(ring_.begin(), ring_.end(), 0.0f);
    writePos_ = 0;
    filled_ = 0;
}

void AudioCapture::latest(float* dst, size_t n) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t avail = std::min(n, filled_);
    std::memset(dst, 0, (n - avail) * sizeof(float));
    float* out = dst + (n - avail);
    // Last `avail` samples end at writePos_.
    size_t start = (writePos_ + kRingSize - avail) % kRingSize;
    size_t first = std::min(avail, kRingSize - start);
    std::memcpy(out, ring_.data() + start, first * sizeof(float));
    std::memcpy(out + first, ring_.data(), (avail - first) * sizeof(float));
}

void AudioCapture::push(const float* samples, size_t n) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < n; i++) {
        ring_[writePos_] = samples[i];
        writePos_ = (writePos_ + 1) % kRingSize;
    }
    filled_ = std::min(filled_ + n, kRingSize);
}

void AudioCapture::onProcess(void* userdata) {
    auto* self = static_cast<AudioCapture*>(userdata);
    pw_buffer* b = pw_stream_dequeue_buffer(self->stream_);
    if (!b) return;

    spa_buffer* buf = b->buffer;
    if (buf->datas[0].data) {
        uint32_t offset = buf->datas[0].chunk->offset;
        uint32_t size = buf->datas[0].chunk->size;
        const float* samples = reinterpret_cast<const float*>(
            static_cast<uint8_t*>(buf->datas[0].data) + offset);
        self->push(samples, size / sizeof(float));
    }
    pw_stream_queue_buffer(self->stream_, b);
}

void AudioCapture::onParamChanged(void* userdata, uint32_t id, const struct spa_pod* param) {
    auto* self = static_cast<AudioCapture*>(userdata);
    if (id != SPA_PARAM_Format || !param) return;

    spa_audio_info_raw info{};
    if (spa_format_audio_raw_parse(param, &info) >= 0 && info.rate > 0)
        self->sampleRate_.store(info.rate);
}
