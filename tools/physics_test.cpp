// Headless physics smoke test: spawn 8 balls + 8 boxes, drive scripted band
// levels (with peak-hold ballistics emulated) for 600 fixed steps, and assert
// every reported object stays finite, inside the world bounds, never lingers
// trapped below the bar/gap surface, and that the object cap holds. No
// GTK/Skia/audio required.
#include "core/models/MeterLayout.h"
#include "physics/Box2dPhysicsWorld.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kDt = 1.0f / 60.0f;

// No part of an object may rest below the bar/gap surface line. Box2D allows
// ~0.5 px (linear slop) of contact penetration and spikes cause brief deeper
// overlap, so only sustained penetration past this depth counts as trapped.
// Without the solid gap fillers a ball wedges 3-4.5 px into the slot corners.
constexpr float kTrapDepthPx = 2.5f;
constexpr int kTrapFrames = 90; // 1.5 s

bool inBounds(const PhysicsObject& o) {
    return std::isfinite(o.x) && std::isfinite(o.y) && std::isfinite(o.angle) &&
           o.x >= -100.0f && o.x <= 1740.0f && o.y >= -300.0f && o.y <= 660.0f;
}

// Interpolated gap-surface y at x, or false when x is not over a gap slot.
bool gapSurfaceAt(float x, const MeterState& meter, float& surfaceY) {
    namespace ml = meterlayout;
    for (int b = 0; b < MeterState::kNumBands - 1; b++) {
        const float leftX = ml::barCenterX(b) + ml::kBarW / 2;
        const float rightX = ml::barCenterX(b + 1) - ml::kBarW / 2;
        if (x <= leftX || x >= rightX) continue;
        const float t = (x - leftX) / (rightX - leftX);
        const float y1 = ml::barTopForLevel(meter.levels[b]);
        const float y2 = ml::barTopForLevel(meter.levels[b + 1]);
        surfaceY = y1 + (y2 - y1) * t;
        return true;
    }
    return false;
}
} // namespace

int main() {
    Box2dPhysicsWorld world;

    for (int i = 0; i < 8; i++) {
        world.spawnBall(-1.0f, -1.0f); // random x, raised above bars
        world.spawnBox(-1.0f, -1.0f);
    }

    MeterState meter;
    std::array<float, MeterState::kNumBands> peakVel{};
    std::array<int, 16> framesBelowSurface{};
    int failures = 0;

    for (int frame = 0; frame < 600; frame++) {
        float t = frame * kDt;
        meter.peakHoldEnabled = frame >= 300; // exercise the enable/disable path

        for (int b = 0; b < MeterState::kNumBands; b++) {
            // Per-band sine pulses through the analyzer-style ballistics.
            float v = 0.5f + 0.5f * std::sin(t * (1.0f + 0.37f * b) + b);
            if (v > meter.levels[b])
                meter.levels[b] = v;
            else
                meter.levels[b] += (v - meter.levels[b]) * 0.22f;

            if (meter.levels[b] >= meter.peaks[b]) {
                meter.peaks[b] = meter.levels[b];
                peakVel[b] = 0.0f;
            } else {
                peakVel[b] += 0.0009f;
                meter.peaks[b] = std::max(meter.levels[b], meter.peaks[b] - peakVel[b]);
            }
        }

        world.step(kDt, meter);

        PhysicsState state = world.state();
        if (state.objects.size() > 16) {
            std::fprintf(stderr, "frame %d: object cap exceeded (%zu)\n", frame,
                         state.objects.size());
            failures++;
        }
        for (std::size_t i = 0; i < state.objects.size(); i++) {
            const PhysicsObject& o = state.objects[i];
            if (!inBounds(o)) {
                std::fprintf(stderr,
                             "frame %d: object out of bounds (x=%g y=%g angle=%g)\n",
                             frame, o.x, o.y, o.angle);
                failures++;
            }

            // Gap-trap regression: the solid fillers must never leave an
            // object's bottom resting below the visual surface line for long.
            float surfaceY = 0.0f;
            const bool belowSurface =
                gapSurfaceAt(o.x, meter, surfaceY) &&
                o.y + o.size > surfaceY + kTrapDepthPx;
            framesBelowSurface[i] = belowSurface ? framesBelowSurface[i] + 1 : 0;
            if (framesBelowSurface[i] == kTrapFrames) {
                std::fprintf(stderr,
                             "frame %d: object %zu trapped below gap surface "
                             "(x=%g y=%g bottom=%g surface=%g)\n",
                             frame, i, o.x, o.y, o.y + o.size, surfaceY);
                failures++;
            }
        }
        if (failures > 10) break; // enough evidence
    }

    // Phase 2: hold every band at a constant level so the meter is a flat
    // raised platform with open slots between bars, and drop a ball over
    // every gap center. Without the solid gap fillers, settled balls wedge
    // 3-4.5 px into the slot corners and rest there; with them the platform
    // is gapless and every ball rests on the surface line.
    for (int b = 0; b < MeterState::kNumBands; b++) {
        meter.levels[b] = 0.5f;
        meter.peaks[b] = 0.5f;
    }
    meter.peakHoldEnabled = false;
    for (int g = 0; g < MeterState::kNumBands - 1; g++)
        world.spawnBall(meterlayout::barCenterX(g) + meterlayout::kColW / 2, 100.0f);
    framesBelowSurface.fill(0);

    for (int frame = 0; frame < 600; frame++) {
        world.step(kDt, meter);
        PhysicsState state = world.state();
        for (std::size_t i = 0; i < state.objects.size(); i++) {
            const PhysicsObject& o = state.objects[i];
            float surfaceY = 0.0f;
            const bool belowSurface =
                gapSurfaceAt(o.x, meter, surfaceY) &&
                o.y + o.size > surfaceY + kTrapDepthPx;
            framesBelowSurface[i] = belowSurface ? framesBelowSurface[i] + 1 : 0;
            if (framesBelowSurface[i] == kTrapFrames) {
                std::fprintf(stderr,
                             "phase2 frame %d: object %zu rests below gap surface "
                             "(x=%g y=%g bottom=%g surface=%g)\n",
                             frame, i, o.x, o.y, o.y + o.size, surfaceY);
                failures++;
            }
        }
    }

    // Eviction: spawning past the cap must drop the oldest objects.
    for (int i = 0; i < 8; i++) world.spawnBall(-1.0f, -1.0f);
    if (world.state().objects.size() > 16) {
        std::fprintf(stderr, "eviction failed: %zu objects\n",
                     world.state().objects.size());
        failures++;
    }

    world.clear();
    if (!world.state().objects.empty()) {
        std::fprintf(stderr, "clear() left %zu objects\n", world.state().objects.size());
        failures++;
    }

    if (failures) {
        std::fprintf(stderr, "physics-test FAILED (%d failures)\n", failures);
        return 1;
    }
    std::printf("physics-test OK\n");
    return 0;
}
