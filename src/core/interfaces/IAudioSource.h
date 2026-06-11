#pragma once

#include <cstddef>
#include <cstdint>

class IAudioSource {
public:
    virtual ~IAudioSource() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool running() const = 0;

    virtual void latest(float* dst, size_t n) = 0;
    virtual uint32_t sampleRate() const = 0;
};
