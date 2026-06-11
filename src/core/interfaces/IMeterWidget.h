#pragma once

#include "../models/MeterState.h"

struct _GtkWidget;
typedef struct _GtkWidget GtkWidget;

class IMeterWidget {
public:
    virtual ~IMeterWidget() = default;

    virtual void updateState(const MeterState& state) = 0;
    virtual GtkWidget* widget() const = 0;
};
