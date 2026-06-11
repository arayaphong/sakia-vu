#pragma once

#include <memory>

#include "IMeterWidget.h"

class IMeterWidgetFactory {
public:
    virtual ~IMeterWidgetFactory() = default;

    virtual std::unique_ptr<IMeterWidget> create() const = 0;
};
