#pragma once

#include <cstddef>
#include <cstdint>
#include "../models/MeterState.h"

class ISpectrumAnalyzer {
public:
    virtual ~ISpectrumAnalyzer() = default;

    virtual size_t sampleCount() const = 0;
    virtual void setSampleRate(uint32_t rate) = 0;
    virtual void update(const float* samples, float gain, bool peakHold) = 0;
    
    virtual void reset() = 0;
    virtual void resetPeaks() = 0;

    virtual MeterState getState() const = 0;
};
