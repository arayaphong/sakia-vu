#include "PipeWireAudioCapture.h"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <cstring>
#include <string>
#include <utility>

namespace {
constexpr size_t kRingSize = 16384; // > FFT size, ~340 ms at 48 kHz

const pw_stream_events kStreamEvents = {
    .version = PW_VERSION_STREAM_EVENTS,
    .param_changed = PipeWireAudioCapture::onParamChanged,
    .process = PipeWireAudioCapture::onProcess,
};

struct DeviceEnumeration {
    pw_thread_loop* loop = nullptr;
    AudioCaptureMode mode = AudioCaptureMode::Microphone;
    std::vector<AudioDevice> devices;
    int pendingSeq = 0;
    bool done = false;
};

const char* lookupProp(const spa_dict* props, const char* key) {
    return props ? spa_dict_lookup(props, key) : nullptr;
}

std::string firstNonEmpty(std::initializer_list<const char*> values) {
    for (const char* value : values) {
        if (value && value[0] != '\0') {
            return value;
        }
    }
    return {};
}

void onRegistryGlobal(void* data, uint32_t id, uint32_t, const char* type, uint32_t,
                      const spa_dict* props) {
    if (std::strcmp(type, PW_TYPE_INTERFACE_Node) != 0) {
        return;
    }

    const char* mediaClass = lookupProp(props, PW_KEY_MEDIA_CLASS);
    auto* state = static_cast<DeviceEnumeration*>(data);
    const char* expectedClass =
        state->mode == AudioCaptureMode::Output ? "Audio/Sink" : "Audio/Source";
    if (!mediaClass || std::strcmp(mediaClass, expectedClass) != 0) {
        return;
    }

    std::string targetObject =
        firstNonEmpty({lookupProp(props, PW_KEY_OBJECT_SERIAL), lookupProp(props, PW_KEY_NODE_NAME)});
    if (targetObject.empty()) {
        targetObject = std::to_string(id);
    }

    std::string displayName = firstNonEmpty({lookupProp(props, PW_KEY_NODE_DESCRIPTION),
                                             lookupProp(props, PW_KEY_NODE_NICK),
                                             lookupProp(props, PW_KEY_NODE_NAME)});
    if (displayName.empty()) {
        displayName = "Input " + std::to_string(id);
    }

    state->devices.push_back({std::move(targetObject), std::move(displayName)});
}

void onCoreDone(void* data, uint32_t, int seq) {
    auto* state = static_cast<DeviceEnumeration*>(data);
    if (seq == state->pendingSeq) {
        state->done = true;
        pw_thread_loop_signal(state->loop, false);
    }
}

const pw_registry_events kRegistryEvents = {
    .version = PW_VERSION_REGISTRY_EVENTS,
    .global = onRegistryGlobal,
};

const pw_core_events kCoreEvents = {
    .version = PW_VERSION_CORE_EVENTS,
    .done = onCoreDone,
};
} // namespace

PipeWireAudioCapture::PipeWireAudioCapture() : ring_(kRingSize, 0.0f) {
    pw_init(nullptr, nullptr);
}

PipeWireAudioCapture::~PipeWireAudioCapture() {
    stop();
    pw_deinit();
}

bool PipeWireAudioCapture::start() {
    if (running_) return true;

    loop_ = pw_thread_loop_new("sakia-audio", nullptr);
    if (!loop_) return false;

    pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_APP_NAME, "SakiaVU",
        nullptr);
    if (captureMode_ == AudioCaptureMode::Output) {
        pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");
    }
    if (!targetObject_.empty()) {
        pw_properties_set(props, PW_KEY_TARGET_OBJECT, targetObject_.c_str());
    }

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

void PipeWireAudioCapture::stop() {
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

void PipeWireAudioCapture::latest(float* dst, size_t n) {
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

std::vector<AudioDevice> PipeWireAudioCapture::devices() {
    DeviceEnumeration state;
    state.mode = captureMode_;
    state.loop = pw_thread_loop_new("sakia-devices", nullptr);
    if (!state.loop) {
        return {};
    }

    pw_thread_loop_lock(state.loop);
    pw_context* context = pw_context_new(pw_thread_loop_get_loop(state.loop), nullptr, 0);
    if (!context) {
        pw_thread_loop_unlock(state.loop);
        pw_thread_loop_destroy(state.loop);
        return {};
    }

    pw_core* core = pw_context_connect(context, nullptr, 0);
    if (!core) {
        pw_context_destroy(context);
        pw_thread_loop_unlock(state.loop);
        pw_thread_loop_destroy(state.loop);
        return {};
    }

    spa_hook coreListener{};
    pw_core_add_listener(core, &coreListener, &kCoreEvents, &state);

    pw_registry* registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
    if (!registry) {
        spa_hook_remove(&coreListener);
        pw_core_disconnect(core);
        pw_context_destroy(context);
        pw_thread_loop_unlock(state.loop);
        pw_thread_loop_destroy(state.loop);
        return {};
    }

    spa_hook registryListener{};
    pw_registry_add_listener(registry, &registryListener, &kRegistryEvents, &state);

    bool started = false;
    if (pw_thread_loop_start(state.loop) >= 0) {
        started = true;
        state.pendingSeq = pw_core_sync(core, PW_ID_CORE, 0);
        while (!state.done) {
            pw_thread_loop_wait(state.loop);
        }
    }

    if (started) {
        pw_thread_loop_unlock(state.loop);
        pw_thread_loop_stop(state.loop);
        pw_thread_loop_lock(state.loop);
    }

    spa_hook_remove(&registryListener);
    pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry));
    spa_hook_remove(&coreListener);
    pw_core_disconnect(core);
    pw_context_destroy(context);
    pw_thread_loop_unlock(state.loop);
    pw_thread_loop_destroy(state.loop);
    return state.devices;
}

void PipeWireAudioCapture::setDeviceTarget(std::string targetObject) {
    targetObject_ = std::move(targetObject);
}

void PipeWireAudioCapture::setCaptureMode(AudioCaptureMode mode) {
    captureMode_ = mode;
}

void PipeWireAudioCapture::push(const float* samples, size_t n) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < n; i++) {
        ring_[writePos_] = samples[i];
        writePos_ = (writePos_ + 1) % kRingSize;
    }
    filled_ = std::min(filled_ + n, kRingSize);
}

void PipeWireAudioCapture::onProcess(void* userdata) {
    auto* self = static_cast<PipeWireAudioCapture*>(userdata);
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

void PipeWireAudioCapture::onParamChanged(void* userdata, uint32_t id, const struct spa_pod* param) {
    auto* self = static_cast<PipeWireAudioCapture*>(userdata);
    if (id != SPA_PARAM_Format || !param) return;

    spa_audio_info_raw info{};
    if (spa_format_audio_raw_parse(param, &info) >= 0 && info.rate > 0)
        self->sampleRate_.store(info.rate);
}
