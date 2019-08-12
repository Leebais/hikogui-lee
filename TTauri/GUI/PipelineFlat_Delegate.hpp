// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#include <boost/exception/all.hpp>
#include <gsl/gsl>

namespace TTauri::GUI::PipelineFlat {

struct Vertex;

struct Delegate {
    struct Error : virtual boost::exception, virtual std::exception {};

    virtual void pipelineFlatPlaceVertices(gsl::span<Vertex> &vertices, int &offset) = 0;
};

}