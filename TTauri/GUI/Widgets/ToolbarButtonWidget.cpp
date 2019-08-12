// Copyright 2019 Pokitec
// All rights reserved.

#include "ToolbarButtonWidget.hpp"
#include "utils.hpp"
#include <cmath>
#include <boost/math/constants/constants.hpp>
#include <typeinfo>

namespace TTauri::GUI::Widgets {

using namespace std::literals;

ToolbarButtonWidget::ToolbarButtonWidget(Draw::Path icon, std::function<void()> delegate) noexcept :
    Widget(), delegate(delegate)
{
    icon.tryRemoveLayers();
    this->icon = std::move(icon);
}

void ToolbarButtonWidget::setParent(Widget *parent) noexcept
{
    Widget::setParent(parent);

    window->addConstraint(box.height == box.width);
}

int ToolbarButtonWidget::state() const noexcept {
    int r = 0;
    r |= window->active ? 1 : 0;
    r |= hover ? 2 : 0;
    r |= pressed ? 4 : 0;
    r |= enabled ? 8 : 0;
    return r;
}


void ToolbarButtonWidget::pipelineImagePlaceVertices(gsl::span<GUI::PipelineImage::Vertex>& vertices, int& offset) noexcept
{
    required_assert(window);
    backingImage.loadOrDraw(*window, box.currentExtent(), [&](auto image) {
        return drawImage(image);
    }, "ToolbarButtonWidget", this, state());

    if (backingImage.image) {
        let currentScale = box.currentExtent() / extent2{backingImage.image->extent};

        GUI::PipelineImage::ImageLocation location;
        location.depth = depth + 0.0f;
        location.origin = {0.0, 0.0};
        location.position = box.currentPosition() + location.origin;
        location.scale = currentScale;
        location.rotation = 0.0;
        location.alpha = 1.0;
        location.clippingRectangle = box.currentRectangle();

        backingImage.image->placeVertices(location, vertices, offset);
    }

    Widget::pipelineImagePlaceVertices(vertices, offset);
}

PipelineImage::Backing::ImagePixelMap ToolbarButtonWidget::drawImage(std::shared_ptr<GUI::PipelineImage::Image> image) noexcept
{
    auto linearMap = Draw::PixelMap<wsRGBA>{image->extent};
    if (pressed) {
        fill(linearMap, pressedBackgroundColor);
    } else if (hover && enabled) {
        fill(linearMap, hoverBackgroundColor);
    } else {
        fill(linearMap);
    }

    let iconSize = numeric_cast<float>(image->extent.height());
    let iconLocation = glm::vec2{image->extent.width() / 2.0f, image->extent.height() / 2.0f};

    auto iconImage = Draw::PixelMap<wsRGBA>{image->extent};
    if (std::holds_alternative<Draw::Path>(icon)) {
        let p = Draw::Alignment::MiddleCenter + T2D(iconLocation, iconSize) * Draw::PathString{std::get<Draw::Path>(icon)};

        fill(iconImage);
        composit(iconImage, p.toPath(wsRGBA{ 0xffffffff }), Draw::SubpixelOrientation::RedLeft);
    } else {
        no_default;
    }

    if (!(hover || window->active)) {
        desaturate(iconImage, 0.5f);
    }

    composit(linearMap, iconImage);
    return { std::move(image), std::move(linearMap) };
}

void ToolbarButtonWidget::handleMouseEvent(MouseEvent event) noexcept {
    hover = event.type != MouseEvent::Type::Exited;

    if (enabled) {
        window->setCursor(Cursor::Clickable);
        pressed = event.down.leftButton;

        if (event.type == MouseEvent::Type::ButtonUp && event.cause.leftButton) {
            delegate();
        }

    } else {
        window->setCursor(Cursor::Default);
    }
}

}