#pragma once

#include <gtk/gtk.h>
#include "../models/MeterState.h"

class IMeterWidget {
public:
    virtual ~IMeterWidget() = default;

    virtual void updateState(const MeterState& state) = 0;
    virtual GtkWidget* widget() const = 0;
};
