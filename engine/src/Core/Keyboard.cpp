#include "Engine/Core/Keyboard.h"
#include "Engine/Core/Window.h"
#include "Engine/Util/ErrorHandling.h"
#include <tbx/bitwise_enum.h>
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <Windows.h>
#include <array>
#include <imgui.h>
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()

namespace Core {

Keyboard::Keyboard(Window& window)
    : m_keysDownPrevFrame(256, false)
    , m_keysDown(256, false)
{
    // https://docs.microsoft.com/en-us/windows/desktop/inputdev/using-raw-input
    std::array<RAWINPUTDEVICE, 1> rid;
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x06;
    rid[0].dwFlags = RIDEV_NOLEGACY; // Adds HID keyboard and also ignores legacy keyboard messages.
    rid[0].hwndTarget = window.hWnd;

    if (!RegisterRawInputDevices(rid.data(), static_cast<UINT>(rid.size()), sizeof(decltype(rid)::value_type))) {
        spdlog::error("Failed to register raw input keyboard device");
    } else {
        spdlog::info("Successfully registered raw input keyboard device");
    }

    // NOTE(Mathijs): we still want to receive legacy signals such that ImGui can pick them up.
    rid[0].dwFlags = 0;
    if (!RegisterRawInputDevices(rid.data(), static_cast<UINT>(rid.size()), sizeof(decltype(rid)::value_type))) {
        spdlog::error("Failed to register raw input keyboard device");
    } else {
        spdlog::info("Successfully registered raw input keyboard device");
    }

    Util::AssertFalse(window.pKeyboard);
    window.pKeyboard = this;
}

bool Keyboard::isKeyPress(Key key) const
{
    if (m_ignoreImGuiEvents && ImGui::GetIO().WantCaptureKeyboard)
        return false;

    return m_keysDown[Tbx::to_underlying(key)] && !m_keysDownPrevFrame[Tbx::to_underlying(key)];
}

bool Keyboard::isKeyRelease(Key key) const
{
    if (m_ignoreImGuiEvents && ImGui::GetIO().WantCaptureKeyboard)
        return false;

    return !m_keysDown[Tbx::to_underlying(key)] && m_keysDownPrevFrame[Tbx::to_underlying(key)];
}

bool Keyboard::isKeyDown(Key key) const
{
    if (m_ignoreImGuiEvents && ImGui::GetIO().WantCaptureKeyboard)
        return false;

    return m_keysDown[Tbx::to_underlying(key)];
}

bool Keyboard::isKeyUp(Key key) const
{
    if (m_ignoreImGuiEvents && ImGui::GetIO().WantCaptureKeyboard)
        return true;

    return !m_keysDown[Tbx::to_underlying(key)];
}

void Keyboard::setIgnoreImGuiEvents(bool ignoreImGuiEvents)
{
    m_ignoreImGuiEvents = ignoreImGuiEvents;
}

void Keyboard::nextFrame()
{
    std::copy(std::begin(m_keysDown), std::end(m_keysDown), std::begin(m_keysDownPrevFrame));
}

void Keyboard::ProcessMessage(UINT uMsg, WPARAM /* wParam */, LPARAM lParam)
{
    if (uMsg == WM_INPUT) {
        UINT dwSize;
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
        std::array<BYTE, sizeof(RAWINPUT)> lpb;
        if (dwSize > lpb.size()) {
            spdlog::error("Keyboard::ProcessMessage => preallocated lpd array is too small");
            return;
        }

        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, lpb.data(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
            spdlog::error("Keyboard::ProcessMessage => GetRawInputData does not return correct size");
            return;
        }

        auto* raw = reinterpret_cast<RAWINPUT*>(lpb.data());
        if (raw->header.dwType == RIM_TYPEKEYBOARD) {
            /*spdlog::info("Keyboard: make={} Flags:{} Reserved:{} ExtraInformation:{}, msg={} VK={} \n",
                raw->data.keyboard.MakeCode,
                raw->data.keyboard.Flags,
                raw->data.keyboard.Reserved,
                raw->data.keyboard.ExtraInformation,
                raw->data.keyboard.Message,
                raw->data.keyboard.VKey);*/
            if ((raw->data.keyboard.Flags & RI_KEY_BREAK) == RI_KEY_BREAK) {
                keyUp(Key { static_cast<unsigned char>(raw->data.keyboard.VKey) });
            } else { // if (raw->data.keyboard.Flags & RI_KEY_MAKE == RI_KEY_MAKE)
                keyDown(Key { static_cast<unsigned char>(raw->data.keyboard.VKey) });
            }
        }
    }
}

void Keyboard::keyDown(Key key)
{
    m_keysDown[Tbx::to_underlying(key)] = true;
}

void Keyboard::keyUp(Key key)
{
    m_keysDown[Tbx::to_underlying(key)] = false;
}
}
