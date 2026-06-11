#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <fftw3.h>

// Converts a 4096-sample window into 16 log-spaced band levels (0..1),
// replicating WebAudio AnalyserNode behaviour from the HTML reference:
// Blackman window, per-bin magnitude smoothing (tau = 0.6), dB mapping
// [-100, -30] -> [0, 1], then attack/release ballistics and peak hold.
class SpectrumAnalyzer {
public:
    static constexpr int kNumBands = 16;
    static constexpr int kFftSize = 4096;
    static constexpr float kFMin = 30.0f;
    static constexpr float kFMax = 16000.0f;

    SpectrumAnalyzer();
    ~SpectrumAnalyzer();

    void setSampleRate(uint32_t rate);

    // samples: kFftSize mono floats. Call once per animation frame.
    void update(const float* samples, float gain, bool peakHold);

    void reset();
    void resetPeaks();

    const std::array<float, kNumBands>& levels() const { return levels_; }
    const std::array<float, kNumBands>& peaks() const { return peaks_; }
    const std::array<std::string, kNumBands>& labels() const { return labels_; }

private:
    void buildBandEdges();

    uint32_t sampleRate_ = 48000;

    float* in_ = nullptr;
    fftwf_complex* out_ = nullptr;
    fftwf_plan plan_ = nullptr;

    std::vector<float> window_;    // Blackman coefficients
    std::vector<float> smoothMag_; // per-bin smoothed magnitudes

    std::array<int, kNumBands + 1> bandEdges_{};
    std::array<std::string, kNumBands> labels_;

    std::array<float, kNumBands> levels_{};
    std::array<float, kNumBands> peaks_{};
    std::array<float, kNumBands> peakVel_{};
};
