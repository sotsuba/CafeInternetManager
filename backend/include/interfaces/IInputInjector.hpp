#pragma once
#include "common/Result.hpp"

namespace interfaces {

    enum class MouseButton {
        Left,
        Right,
        Middle
    };

    enum class KeyCode {
        Unknown = 0,
        // Alphanumeric
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        // Controls
        Enter, Space, Backspace, Tab, Escape,
        Shift, Control, Alt, Meta, // Modifier keys
        // Navigation
        Left, Right, Up, Down,
        Home, End, PageUp, PageDown,
        Insert, Delete,
        // Function keys
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        // Lock keys
        CapsLock, NumLock, ScrollLock,
        // Symbols (Basic map)
        Comma, Period, Slash, Semicolon, Quote, BracketLeft, BracketRight,
        Backslash, Minus, Equal, Tilde
    };

    class IInputInjector {
    public:
        virtual ~IInputInjector() = default;

        // Move mouse to absolute position (0.0 to 1.0)
        virtual common::EmptyResult move_mouse(float x_percent, float y_percent) = 0;

        // Click mouse button
        virtual common::EmptyResult click_mouse(MouseButton button, bool is_down) = 0;

        // Scroll mouse wheel (positive = up, negative = down)
        virtual common::EmptyResult scroll_mouse(int delta) = 0;

        // Key Press (down/up)
        virtual common::EmptyResult press_key(KeyCode key, bool is_down) = 0;

        // Send Text (Unicode)
        virtual common::EmptyResult send_text(const std::string& text) = 0;
    };

}
