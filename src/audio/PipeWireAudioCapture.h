#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "core/interfaces/IAudioSource.h"

struct pw_thread_loop;
struct pw_stream;

// Captures mono float samples from the default input via PipeWire on a
// dedicated thread and keeps the most recent window available for the UI.
class PipeWireAudioCapture final : public IAudioSource {
public:
    PipeWireAudioCapture();
    ~PipeWireAudioCapture() override;

    bool start() override;
    void stop() override;
    bool running() const override { return running_; }

    // Copies the latest `n` samples into dst (zero-padded if fewer arrived).
    void latest(float* dst, size_t n) override;

    uint32_t sampleRate() const override { return sampleRate_.load(); }

    static void onProcess(void* userdata);
    static void onParamChanged(void* userdata, uint32_t id, const struct spa_pod* param);

private:
    void push(const float* samples, size_t n);

    pw_thread_loop* loop_ = nullptr;
    pw_stream* stream_ = nullptr;
    bool running_ = false;

    std::atomic<uint32_t> sampleRate_{48000};

    std::mutex mutex_;
    std::vector<float> ring_;
    size_t writePos_ = 0;
    size_t filled_ = 0;
};
