#include "FftwSpectrumAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kMinDb = -100.0f;
constexpr float kMaxDb = -30.0f;
constexpr float kSmoothing = 0.6f; // WebAudio smoothingTimeConstant
} // namespace

FftwSpectrumAnalyzer::FftwSpectrumAnalyzer() {
    in_ = fftwf_alloc_real(kFftSize);
    out_ = fftwf_alloc_complex(kFftSize / 2 + 1);
    plan_ = fftwf_plan_dft_r2c_1d(kFftSize, in_, out_, FFTW_MEASURE);

    window_.resize(kFftSize);
    for (int i = 0; i < kFftSize; i++) {
        double a = 2.0 * M_PI * i / (kFftSize - 1);
        window_[i] = static_cast<float>(0.42 - 0.5 * std::cos(a) + 0.08 * std::cos(2.0 * a));
    }
    smoothMag_.assign(kFftSize / 2, 0.0f);

    buildBandEdges();
}

FftwSpectrumAnalyzer::~FftwSpectrumAnalyzer() {
    fftwf_destroy_plan(plan_);
    fftwf_free(in_);
    fftwf_free(out_);
}

void FftwSpectrumAnalyzer::setSampleRate(uint32_t rate) {
    if (rate == sampleRate_ || rate == 0) return;
    sampleRate_ = rate;
    buildBandEdges();
}

void FftwSpectrumAnalyzer::buildBandEdges() {
    const int binCount = kFftSize / 2;
    const float hzPerBin = (sampleRate_ / 2.0f) / binCount;
    for (int i = 0; i <= kNumBands; i++) {
        float f = kFMin * std::pow(kFMax / kFMin, static_cast<float>(i) / kNumBands);
        bandEdges_[i] = static_cast<int>(std::lround(f / hzPerBin));
        if (i < kNumBands) {
            float fc = kFMin * std::pow(kFMax / kFMin, (i + 0.5f) / kNumBands);
            char buf[16];
            if (fc >= 10000.0f)
                std::snprintf(buf, sizeof(buf), "%.0fk", fc / 1000.0f);
            else if (fc >= 1000.0f)
                std::snprintf(buf, sizeof(buf), "%.1fk", fc / 1000.0f);
            else
                std::snprintf(buf, sizeof(buf), "%.0f", fc);
            labels_[i] = buf;
        }
    }
}

void FftwSpectrumAnalyzer::update(const float* samples, float gain, bool peakHold) {
    for (int i = 0; i < kFftSize; i++)
        in_[i] = samples[i] * window_[i];
    fftwf_execute(plan_);

    const int binCount = kFftSize / 2;
    for (int k = 0; k < binCount; k++) {
        float mag = std::hypot(out_[k][0], out_[k][1]) / kFftSize;
        smoothMag_[k] = kSmoothing * smoothMag_[k] + (1.0f - kSmoothing) * mag;
    }

    for (int b = 0; b < kNumBands; b++) {
        int lo = bandEdges_[b];
        int hi = std::max(bandEdges_[b + 1], lo + 1);
        float sum = 0.0f;
        int n = 0;
        for (int k = lo; k < hi && k < binCount; k++, n++) {
            float db = smoothMag_[k] > 0.0f ? 20.0f * std::log10(smoothMag_[k]) : kMinDb;
            sum += std::clamp((db - kMinDb) / (kMaxDb - kMinDb), 0.0f, 1.0f);
        }
        float v = std::min(1.0f, (n ? sum / n : 0.0f) * gain);

        // Fast attack, slow release.
        if (v > levels_[b])
            levels_[b] = v;
        else
            levels_[b] += (v - levels_[b]) * 0.22f;

        // Peak hold falls under gravity.
        if (levels_[b] >= peaks_[b]) {
            peaks_[b] = levels_[b];
            peakVel_[b] = 0.0f;
        } else {
            peakVel_[b] += 0.0009f;
            peaks_[b] = std::max(levels_[b], peaks_[b] - peakVel_[b]);
        }
        if (!peakHold) peaks_[b] = 0.0f;
    }
}

void FftwSpectrumAnalyzer::reset() {
    levels_.fill(0.0f);
    resetPeaks();
    std::fill(smoothMag_.begin(), smoothMag_.end(), 0.0f);
}

void FftwSpectrumAnalyzer::resetPeaks() {
    peaks_.fill(0.0f);
    peakVel_.fill(0.0f);
}

MeterState FftwSpectrumAnalyzer::getState() const {
    MeterState state;
    state.levels = levels_;
    state.peaks = peaks_;
    state.labels = labels_;
    return state;
}
