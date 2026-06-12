#pragma once

#include <functional>

#include "../models/MeterState.h"
#include "../models/PhysicsState.h"

struct _GtkWidget;
typedef struct _GtkWidget GtkWidget;

class IMeterWidget {
public:
    virtual ~IMeterWidget() = default;

    virtual void updateState(const MeterState& state) = 0;
    virtual void updatePhysicsState(const PhysicsState& state) = 0;

    // Invoked on canvas clicks with logical-canvas coords (1640x560 space);
    // secondary is true for right-clicks.
    virtual void setSpawnCallback(
        std::function<void(float lx, float ly, bool secondary)> cb) = 0;

    virtual GtkWidget* widget() const = 0;
};
