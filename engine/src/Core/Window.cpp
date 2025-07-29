#include "Engine/Core/Window.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()
#include "Engine/Core/Keyboard.h"
#include "Engine/Core/Mouse.h"
#include <unordered_map>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Core {

Window::Window(const std::string& title, const glm::uvec2& size, HINSTANCE hInstance, int nCmdShow)
    : hWnd(createWindow(title, size, hInstance, nCmdShow))
    , shouldClose(false)
    , size(size)
    , m_isFullScreen(false)
{
    // Store pointer to this specific Window instance in the Windows window user data.
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Store the current window dimensions so they can be restored
    // when switching out of the full screen state.
    updateWindowPositionAndSize();

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
}

Window::~Window()
{
    DestroyWindow(hWnd);
}

void Window::setFullScreen(bool bFullScreen)
{
    if (m_isFullScreen != bFullScreen) {
        m_isFullScreen = bFullScreen;

        if (m_isFullScreen) {
            // Store the current window dimensions so they can be restored
            // when switching out of the full screen state.
            updateWindowPositionAndSize();

            // Set the window style to a border less window.
            UINT uWindowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
            SetWindowLongW(hWnd, GWL_STYLE, uWindowStyle);

            // Query the frame of the nearest display device for the window. This is
            // required to set the full screen dimension of the window when using a
            // multi-monitor setup.
            HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFOEX monitorInfo = {};
            monitorInfo.cbSize = sizeof(decltype(monitorInfo));
            GetMonitorInfo(hMonitor, &monitorInfo);

            int iWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
            int iHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
            SetWindowPos(hWnd, HWND_TOP,
                monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.top,
                iWidth,
                iHeight,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);
            ShowWindow(hWnd, SW_MAXIMIZE);
        } else {
            // Restore all the window decorators.
            SetWindowLong(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

            SetWindowPos(
                hWnd, HWND_NOTOPMOST,
                static_cast<int>(position.x),
                static_cast<int>(position.y),
                static_cast<int>(size.x),
                static_cast<int>(size.y),
                SWP_FRAMECHANGED | SWP_NOACTIVATE);
            ShowWindow(hWnd, SW_NORMAL);
        }
    }
}

bool Window::isFullScreen() const
{
    return m_isFullScreen;
}

static thread_local bool s_invokeImGuiWndProc = false;
void Window::updateInput(bool imgui)
{
    if (pKeyboard)
        pKeyboard->nextFrame();
    if (pMouse)
        pMouse->nextFrame();

    s_invokeImGuiWndProc = imgui;

    MSG msg = {};
    while (true) {
        if (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            break;
        }
    }
}

void Window::popup(const std::string& title, const std::string& text)
{
    MessageBox(hWnd, text.c_str(), title.c_str(), MB_OK | MB_ICONQUESTION);
}

HWND Window::createWindow(const std::string& title, const glm::uvec2& size, HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Window::WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 2);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = "MyWindowClassName";
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    // Register this Window configuration to Windows as a "windows class".
    if (!RegisterClassEx(&wc)) {
        spdlog::error("Error registering window class");
        throw;
    }

    RECT desiredClientArea { 0, 0, static_cast<LONG>(size.x), static_cast<LONG>(size.y) };
    AdjustWindowRectEx(&desiredClientArea, WS_OVERLAPPEDWINDOW, false, NULL);

    HWND hWnd = CreateWindowEx(
        NULL, // Style
        "MyWindowClassName", // Window class
        title.c_str(),
        WS_OVERLAPPEDWINDOW, // dwStyle
        CW_USEDEFAULT, CW_USEDEFAULT, // x/y
        desiredClientArea.right - desiredClientArea.left, desiredClientArea.bottom - desiredClientArea.top,
        nullptr, // hWndParent
        nullptr, // hMenu
        hInstance,
        nullptr); // lParam
    if (!hWnd) {
        spdlog::error("Error creating window");
        throw;
    }

    return hWnd;
}

LRESULT Window::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto* thisPtr = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    if (s_invokeImGuiWndProc && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;

    switch (uMsg) {
    case WM_DESTROY: {
        thisPtr->shouldClose = true;
        PostQuitMessage(0);
        return 0;
    } break;

    case WM_SIZE: {
        thisPtr->updateWindowPositionAndSize();
        for (auto& resizeCallback : thisPtr->m_resizeCallbacks)
            resizeCallback(thisPtr->size);
    } break;

    case WM_KEYUP:
    case WM_KEYDOWN:
    case WM_INPUT: {
        if (thisPtr->pMouse)
            thisPtr->pMouse->ProcessMessage(uMsg, wParam, lParam);
        if (thisPtr->pKeyboard)
            thisPtr->pKeyboard->ProcessMessage(uMsg, wParam, lParam);
    } break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void Window::updateWindowPositionAndSize()
{
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    size = { clientRect.right - clientRect.left, clientRect.bottom - clientRect.top };

    RECT windowRect;
    GetWindowRect(hWnd, &windowRect);
    position = { windowRect.left, windowRect.top };
}

}
