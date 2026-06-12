#pragma once

#include <cstdint>
#include <vector>

// Render model for the physics playground overlay. Positions/sizes are in
// logical canvas pixels (1640x560 space, y-down), produced by IPhysicsWorld
// and consumed by the renderer.
struct PhysicsObject {
    enum class Kind : std::uint8_t { Ball, Box };

    Kind kind = Kind::Ball;
    float x = 0.0f;     // center x, logical px
    float y = 0.0f;     // center y, logical px
    float angle = 0.0f; // radians, clockwise (y-down space)
    float size = 0.0f;  // Ball: radius px; Box: half-extent px
    float hue = 0.0f;   // degrees, renderer maps to color
};

struct PhysicsState {
    std::vector<PhysicsObject> objects;
};
