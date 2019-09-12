// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#include "TTauri/GUI/Device_forward.hpp"
#include "TTauri/GUI/Window_forward.hpp"
#include "TTauri/Required/required.hpp"
#include <memory>

namespace TTauri::GUI {

class Pipeline_base {
public:
    Window const &window;

    Pipeline_base(Window const &window);

    virtual ~Pipeline_base() = default;
    Pipeline_base(const Pipeline_base &) = delete;
    Pipeline_base &operator=(const Pipeline_base &) = delete;
    Pipeline_base(Pipeline_base &&) = delete;
    Pipeline_base &operator=(Pipeline_base &&) = delete;

    Device const &device() const;

};

}