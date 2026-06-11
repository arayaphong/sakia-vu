#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct AudioDevice {
    std::string targetObject;
    std::string displayName;
};

enum class AudioCaptureMode {
    Microphone,
    Output,
};

class IAudioSource {
public:
    virtual ~IAudioSource() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool running() const = 0;

    virtual void latest(float* dst, size_t n) = 0;
    virtual uint32_t sampleRate() const = 0;

    virtual std::vector<AudioDevice> devices() = 0;
    virtual void setDeviceTarget(std::string targetObject) = 0;
    virtual const std::string& deviceTarget() const = 0;
    virtual void setCaptureMode(AudioCaptureMode mode) = 0;
    virtual AudioCaptureMode captureMode() const = 0;
};
