// Headless physics smoke test: spawn 8 balls + 8 boxes, drive scripted band
// levels (with peak-hold ballistics emulated) for 600 fixed steps, and assert
// every reported object stays finite, inside the world bounds, and that the
// object cap holds. No GTK/Skia/audio required.
#include "core/models/MeterLayout.h"
#include "physics/Box2dPhysicsWorld.h"

#include <cmath>
#include <cstdio>

namespace {
constexpr float kDt = 1.0f / 60.0f;

bool inBounds(const PhysicsObject& o) {
    return std::isfinite(o.x) && std::isfinite(o.y) && std::isfinite(o.angle) &&
           o.x >= -100.0f && o.x <= 1740.0f && o.y >= -300.0f && o.y <= 660.0f;
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
        for (const PhysicsObject& o : state.objects) {
            if (!inBounds(o)) {
                std::fprintf(stderr,
                             "frame %d: object out of bounds (x=%g y=%g angle=%g)\n",
                             frame, o.x, o.y, o.angle);
                failures++;
            }
        }
        if (failures > 10) break; // enough evidence
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
