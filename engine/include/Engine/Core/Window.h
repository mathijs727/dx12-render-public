#pragma once
#include "Engine/Core/ForwardDeclares.h"
#include "Engine/Util/WindowsForwardDeclares.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec2.hpp>
DISABLE_WARNINGS_POP()
#include <functional>
#include <string>
#include <vector>

namespace Core {

class Window {
public:
    HWND hWnd; // Handle to Windows window.

    bool shouldClose;
    glm::uvec2 position;
    glm::uvec2 size;

    Mouse* pMouse = nullptr;
    Keyboard* pKeyboard = nullptr;

public:
    Window(const std::string& title, const glm::uvec2& size, HINSTANCE hInstance, int nCmdShow);
    ~Window();

    template <typename F>
    void registerResizeCallback(F&& callback) { m_resizeCallbacks.push_back(std::forward<F>(callback)); }

    void setFullScreen(bool bFullScreen);
    bool isFullScreen() const;

    void updateInput(bool imgui = false);

    void popup(const std::string& text, const std::string& title);

private:
    static HWND createWindow(const std::string& title, const glm::uvec2& size, HINSTANCE hInstance, int nCmdShow);
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void updateWindowPositionAndSize();

    friend class Keyboard;
    friend class Mouse;

private:
    bool m_isFullScreen;
    std::vector<std::function<void(const glm::uvec2&)>> m_resizeCallbacks;
};
}
