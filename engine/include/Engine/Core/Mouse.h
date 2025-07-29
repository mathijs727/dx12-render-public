#pragma once
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec2.hpp>
DISABLE_WARNINGS_POP()
#include "Engine/Core/ForwardDeclares.h"
#include "Engine/Util/WindowsForwardDeclares.h"

namespace Core {

class Mouse {
public:
    Mouse(Window& window);

    glm::ivec2 getPosition() const;
    glm::ivec2 getPositionDelta() const;

    void setCapture(bool captured);

private:
    void nextFrame();
    void ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    friend class Window;

private:
    HWND m_hWnd;

    glm::ivec2 m_position;
    glm::ivec2 m_prevPosition;

    bool m_captured { false };
    glm::ivec2 m_windowCenter;
};
}
