#pragma once

#include <array>
#include <string>

struct MeterState {
    static constexpr int kNumBands = 16;
    std::array<float, kNumBands> levels{};
    std::array<float, kNumBands> peaks{};
    std::array<std::string, kNumBands> labels;
    bool peakHoldEnabled = false;
};
