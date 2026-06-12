#pragma once

#include "core/interfaces/IPhysicsWorld.h"

#include "box2d/box2d.h"

#include <array>
#include <deque>
#include <random>
#include <utility>

// Box2D-backed playground. The world lives in meters (100 px/m) with the same
// y-down orientation as the logical canvas, so positions and angles map to
// Skia without sign flips. All px<->m conversion stays inside this class.
class Box2dPhysicsWorld final : public IPhysicsWorld {
public:
    Box2dPhysicsWorld();
    ~Box2dPhysicsWorld() override;

    Box2dPhysicsWorld(const Box2dPhysicsWorld&) = delete;
    Box2dPhysicsWorld& operator=(const Box2dPhysicsWorld&) = delete;

    void step(float dtSeconds, const MeterState& meter) override;
    void spawnBall(float lx, float ly) override;
    void spawnBox(float lx, float ly) override;
    void clear() override;
    void setLowGravity(bool low) override;
    PhysicsState state() const override;

private:
    static constexpr float kPxPerMeter = 100.0f;
    static constexpr float kFixedDt = 1.0f / 60.0f;
    static constexpr int kSubSteps = 4;
    static constexpr int kMaxObjects = 16;
    static constexpr float kBarHalfH = 3.0f;  // 6 m tall: bottom never clears ground
    static constexpr float kMaxKinematicSpeed = 30.0f;

    static float toM(float px) { return px / kPxPerMeter; }
    static float toPx(float m) { return m * kPxPerMeter; }

    void syncKinematics(const MeterState& meter);
    void addObject(PhysicsObject::Kind kind, float lx, float ly);
    // Lowest bar-top y (logical px) among bands overlapping [lx-half, lx+half].
    float safeSpawnY(float lx, float halfPx, float ly) const;
    void pruneEscaped();

    b2WorldId world_{};
    std::array<b2BodyId, MeterState::kNumBands> bars_{};
    std::array<b2BodyId, MeterState::kNumBands> peakPlatforms_{};
    bool peaksEnabled_ = false;
    std::array<float, MeterState::kNumBands> lastLevels_{};

    // Oldest-first; PhysicsObject holds the immutable props (kind/size/hue),
    // position and angle are read back from Box2D in state().
    std::deque<std::pair<b2BodyId, PhysicsObject>> objects_;

    float accumulator_ = 0.0f;
    std::mt19937 rng_{0x5a71a0u}; // fixed seed: deterministic for physics-test
};
