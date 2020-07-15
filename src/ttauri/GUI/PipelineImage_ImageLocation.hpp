// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#include "../vec.hpp"
#include "../mat.hpp"
#include "../aarect.hpp"

namespace tt::PipelineImage {

/*! Information on the location and orientation of an image on a window.
*/
struct ImageLocation {
    /** Transformation matrix.
     */
    mat transform;

    //! The position in pixels of the clipping rectangle relative to the top-left corner of the window, and extent in pixels.
    aarect clippingRectangle;

    bool operator==(const ImageLocation &other) noexcept {
        return (
            transform == other.transform &&
            clippingRectangle == other.clippingRectangle
        );
    }

    bool operator!=(const ImageLocation &other) noexcept { return !((*this) == other); }
};

}