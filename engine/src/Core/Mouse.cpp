#include "Engine/Core/Mouse.h"
#include "Engine/Core/Window.h"
#include "Engine/Util/ErrorHandling.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <Windows.h>
#include <array>
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()

namespace Core {

Mouse::Mouse(Window& window)
    : m_hWnd(window.hWnd)
{
    // https://docs.microsoft.com/en-us/windows/desktop/inputdev/using-raw-input
    std::array<RAWINPUTDEVICE, 1> rid;
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x02;
    rid[0].dwFlags = 0; // RIDEV_NOLEGACY; // HID mouse + ignore legacy mouse messages
    rid[0].hwndTarget = window.hWnd;

    if (!RegisterRawInputDevices(rid.data(), static_cast<UINT>(rid.size()), sizeof(decltype(rid)::value_type))) {
        spdlog::error("Failed to register raw input mouse device");
    } else {
        spdlog::info("Successfully registered raw input mouse device");
    }

    POINT p;
    GetCursorPos(&p);
    m_prevPosition = m_position = glm::ivec2(p.x, p.y);
    Util::AssertFalse(window.pMouse);
    window.pMouse = this;
}

glm::ivec2 Mouse::getPosition() const
{
    return m_position;
}

glm::ivec2 Mouse::getPositionDelta() const
{
    return m_position - m_prevPosition;
}

void Mouse::setCapture(bool captured)
{
    m_captured = captured;

    ShowCursor(!captured);
    if (captured) {
        RECT rect;
        GetClientRect(m_hWnd, &rect);

        POINT center;
        center.x = (rect.right - rect.left) / 2;
        center.y = (rect.bottom - rect.top) / 2;
        MapWindowPoints(m_hWnd, nullptr, &center, 1);
        m_windowCenter = glm::ivec2(center.x, center.y);

        SetCursorPos(center.x, center.y);
        m_prevPosition = m_position = m_windowCenter;
    }
}

void Mouse::nextFrame()
{
    m_prevPosition = m_position;
    if (m_captured) {
        SetCursorPos(m_windowCenter.x, m_windowCenter.y);
        m_prevPosition = m_position = m_windowCenter;
    }
}

void Mouse::ProcessMessage(UINT uMsg, WPARAM, LPARAM lParam)
{
    // https://docs.microsoft.com/en-us/windows/desktop/inputdev/using-raw-input
    if (uMsg == WM_INPUT) {
        UINT dwSize;
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
        std::array<BYTE, sizeof(RAWINPUT)> lpb;
        if (dwSize > lpb.size()) {
            spdlog::error("Mouse::ProcessMessage => preallocated lpd array is too small");
            return;
        }

        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, lpb.data(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
            spdlog::error("Mouse::ProcessMessage => GetRawInputData does not return correct size");
            return;
        }

        auto* raw = reinterpret_cast<RAWINPUT*>(lpb.data());
        if (raw->header.dwType == RIM_TYPEMOUSE) {
            /*spdlog::info("Mouse: usFlags={:#4} ulButtons={} usButtonFlags={} usButtonData={} ulRawButtons={} lLastX={} lLastY={} ulExtraInformation={}",
                raw->data.mouse.usFlags,
                raw->data.mouse.ulButtons,
                raw->data.mouse.usButtonFlags,
                raw->data.mouse.usButtonData,
                raw->data.mouse.ulRawButtons,
                raw->data.mouse.lLastX,
                raw->data.mouse.lLastY,
                raw->data.mouse.ulExtraInformation);*/

            m_position += glm::ivec2(raw->data.mouse.lLastX, raw->data.mouse.lLastY);
        }
    }
}
}
