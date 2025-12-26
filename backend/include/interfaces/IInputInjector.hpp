#pragma once
#include "common/Result.hpp"

namespace interfaces {

    enum class MouseButton {
        Left,
        Right,
        Middle
    };

    class IInputInjector {
    public:
        virtual ~IInputInjector() = default;

        // Move mouse to absolute position (0.0 to 1.0)
        virtual common::EmptyResult move_mouse(float x_percent, float y_percent) = 0;

        // Click mouse button
        virtual common::EmptyResult click_mouse(MouseButton button, bool is_down) = 0;

        // Scroll (optional, maybe later)
        // virtual common::EmptyResult scroll(int delta_y) = 0;
    };

}
