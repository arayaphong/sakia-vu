// Headless physics smoke test, three phases plus lifecycle checks:
//   1. 8 balls + 8 boxes under scripted sine band levels (with peak-hold
//      ballistics emulated), 600 fixed steps.
//   2. Flat held level with a ball dropped over every gap center: settled
//      balls must rest on the surface line, not wedged into the slots.
//   3. Square-wave full-band spikes (instant attack, the trap scenario): a
//      surface rising ~50 px/step must never leave an object below it.
// Asserts every reported object stays finite, inside the world bounds, never
// lingers below the bar/gap surface, and that the object cap and clear()
// hold. No GTK/Skia/audio required.
#include "core/models/MeterLayout.h"
#include "physics/Box2dPhysicsWorld.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kDt = 1.0f / 60.0f;

// No part of an object may rest below the bar/gap surface line. Box2D allows
// ~0.5 px (linear slop) of contact penetration, and during spikes the bars
// (clamped to 50 px/step) briefly lag the level-derived surface used here,
// so only sustained penetration past this depth counts as trapped.
constexpr float kTrapDepthPx = 2.5f;
constexpr int kTrapFrames = 90; // 1.5 s

bool inBounds(const PhysicsObject& o) {
    return std::isfinite(o.x) && std::isfinite(o.y) && std::isfinite(o.angle) &&
           o.x >= -100.0f && o.x <= 1740.0f && o.y >= -300.0f && o.y <= 660.0f;
}

// Meter surface y at x (bar top over a column, lerp in a gap slot), or false
// when x is outside the meter strip.
bool surfaceAt(float x, const MeterState& meter, float& surfaceY) {
    namespace ml = meterlayout;
    for (int b = 0; b < MeterState::kNumBands; b++) {
        if (std::abs(ml::barCenterX(b) - x) <= ml::kBarW / 2) {
            surfaceY = ml::barTopForLevel(meter.levels[b]);
            return true;
        }
    }
    for (int b = 0; b < MeterState::kNumBands - 1; b++) {
        const float leftX = ml::barCenterX(b) + ml::kBarW / 2;
        const float rightX = ml::barCenterX(b + 1) - ml::kBarW / 2;
        if (x <= leftX || x >= rightX) continue;
        const float y1 = ml::barTopForLevel(meter.levels[b]);
        const float y2 = ml::barTopForLevel(meter.levels[b + 1]);
        surfaceY = std::max(y1, y2);
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

    auto checkObjects = [&](const char* phase, int frame) {
        PhysicsState state = world.state();
        if (state.objects.size() > 16) {
            std::fprintf(stderr, "%s frame %d: object cap exceeded (%zu)\n", phase,
                         frame, state.objects.size());
            failures++;
        }
        for (std::size_t i = 0; i < state.objects.size(); i++) {
            const PhysicsObject& o = state.objects[i];
            if (!inBounds(o)) {
                std::fprintf(stderr,
                             "%s frame %d: object out of bounds (x=%g y=%g angle=%g)\n",
                             phase, frame, o.x, o.y, o.angle);
                failures++;
            }

            // Trap regression: nothing may rest below the surface line,
            // neither wedged in a gap slot nor embedded inside a bar.
            float surfaceY = 0.0f;
            const bool belowSurface =
                surfaceAt(o.x, meter, surfaceY) &&
                o.y + o.size > surfaceY + kTrapDepthPx;
            framesBelowSurface[i] = belowSurface ? framesBelowSurface[i] + 1 : 0;
            if (framesBelowSurface[i] == kTrapFrames) {
                std::fprintf(stderr,
                             "%s frame %d: object %zu trapped below gap surface "
                             "(x=%g y=%g bottom=%g surface=%g)\n",
                             phase, frame, i, o.x, o.y, o.y + o.size, surfaceY);
                failures++;
            }
        }
    };

    for (int frame = 0; frame < 600 && failures <= 10; frame++) {
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
        checkObjects("phase1", frame);
    }

    // Phase 2: hold every band at a constant level so the meter is a flat
    // raised platform with open slots between bars, and drop a ball over
    // every gap center. Settled balls must rest on the surface line, not
    // wedged 3-4.5 px into the slot corners.
    for (int b = 0; b < MeterState::kNumBands; b++) {
        meter.levels[b] = 0.5f;
        meter.peaks[b] = 0.5f;
    }
    meter.peakHoldEnabled = false;
    for (int g = 0; g < MeterState::kNumBands - 1; g++)
        world.spawnBall(meterlayout::barCenterX(g) + meterlayout::kColW / 2, 100.0f);
    framesBelowSurface.fill(0);

    for (int frame = 0; frame < 600 && failures <= 10; frame++) {
        world.step(kDt, meter);
        checkObjects("phase2", frame);
    }

    // Phase 3: checkerboard square-wave spikes -- every gap has one bar
    // spiking and one near the floor (instant attack, maximal slant), the
    // scenario that embeds wedged objects faster than the solver's clamped
    // depenetration can lift them. Hold each extreme long enough for a
    // genuinely trapped object to trip the sustained-depth counter.
    framesBelowSurface.fill(0);
    for (int frame = 0; frame < 900 && failures <= 10; frame++) {
        const int parity = (frame / 150) % 2;
        for (int b = 0; b < MeterState::kNumBands; b++) {
            meter.levels[b] = b % 2 == parity ? 0.75f : 0.05f;
            meter.peaks[b] = meter.levels[b];
        }
        world.step(kDt, meter);
        checkObjects("phase3", frame);
    }

    // Phase 4: peak-hold ledges sweeping down through the resting pile --
    // an object squeezed between a falling ledge and the gap surface is
    // pressed into the filler and must still surface promptly.
    meter.peakHoldEnabled = true;
    framesBelowSurface.fill(0);
    for (int cycle = 0; cycle < 3; cycle++) {
        for (int b = 0; b < MeterState::kNumBands; b++) {
            meter.levels[b] = 0.1f;
            meter.peaks[b] = 0.8f;
        }
        peakVel.fill(0.0f);
        for (int frame = 0; frame < 200 && failures <= 10; frame++) {
            for (int b = 0; b < MeterState::kNumBands; b++) {
                peakVel[b] += 0.0009f;
                meter.peaks[b] = std::max(meter.levels[b], meter.peaks[b] - peakVel[b]);
            }
            world.step(kDt, meter);
            checkObjects("phase4", cycle * 200 + frame);
        }
    }
    meter.peakHoldEnabled = false;

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
