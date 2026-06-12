#pragma once

#include "../models/MeterState.h"
#include "../models/PhysicsState.h"

// Rigid-body playground driven by the meter: bars (and held peak dots) act as
// kinematic collision bodies that launch dynamic balls/boxes.
class IPhysicsWorld {
public:
    virtual ~IPhysicsWorld() = default;

    // Advance the simulation by dtSeconds (real frame time; implementation
    // sub-steps at a fixed rate). meter.levels drives the bar bodies;
    // meter.peaks + meter.peakHoldEnabled drive the peak-platform bodies.
    virtual void step(float dtSeconds, const MeterState& meter) = 0;

    // Spawn at logical canvas coords; y is raised above bars if needed.
    virtual void spawnBall(float lx, float ly) = 0;
    virtual void spawnBox(float lx, float ly) = 0;

    virtual void clear() = 0;
    virtual void setLowGravity(bool low) = 0;

    virtual PhysicsState state() const = 0;
};
