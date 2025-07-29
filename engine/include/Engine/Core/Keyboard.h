#pragma once
#include "Engine/Core/ForwardDeclares.h"
#include "Engine/Util/WindowsForwardDeclares.h"
#include <vector>

namespace Core {

enum class Key : unsigned char;

class Keyboard {
public:
    Keyboard(Window& window);

    bool isKeyPress(Key key) const;
    bool isKeyRelease(Key key) const;
    bool isKeyDown(Key key) const;
    bool isKeyUp(Key key) const;

    void setIgnoreImGuiEvents(bool ignoreImGuiEvents);

private:
    void nextFrame();
    void ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    friend class Window;

    void keyDown(Key key);
    void keyUp(Key key);

private:
    bool m_ignoreImGuiEvents = true;
    std::vector<bool> m_keysDownPrevFrame;
    std::vector<bool> m_keysDown;
};

// https://docs.microsoft.com/en-us/windows/desktop/inputdev/virtual-key-codes
enum class Key : unsigned char {
    ENTER = 0x0D,
    SHIFT = 0x10,
    ESCAPE = 0x1B,
    CONTROL = 0x11,
    SPACE = 0x20,
    NUMPAD0 = 0x60,
    NUMPAD1 = 0x61,
    NUMPAD2 = 0x62,
    NUMPAD3 = 0x63,
    NUMPAD4 = 0x64,
    NUMPAD5 = 0x65,
    NUMPAD6 = 0x66,
    NUMPAD7 = 0x67,
    NUMPAD8 = 0x68,
    NUMPAD9 = 0x69,
    REGULAR0 = 0x30,
    REGULAR1 = 0x31,
    REGULAR2 = 0x32,
    REGULAR3 = 0x33,
    REGULAR4 = 0x34,
    REGULAR5 = 0x35,
    REGULAR6 = 0x36,
    REGULAR7 = 0x37,
    REGULAR8 = 0x38,
    REGULAR9 = 0x39,
    A = 0x41,
    B = 0x42,
    C = 0x43,
    D = 0x44,
    E = 0x45,
    F = 0x46,
    G = 0x47,
    H = 0x48,
    I = 0x49,
    J = 0x4A,
    K = 0x4B,
    L = 0x4C,
    M = 0x4D,
    N = 0x4E,
    O = 0x4F,
    P = 0x50,
    Q = 0x51,
    R = 0x52,
    S = 0x53,
    T = 0x54,
    U = 0x55,
    V = 0x56,
    W = 0x57,
    X = 0x58,
    Y = 0x59,
    Z = 0x5A,
    UP_ARROW = 0x26,
    DOWN_ARROW = 0x28,
    LEFT_ARROW = 0x25,
    RIGHT_ARROW = 0x27
};
}
