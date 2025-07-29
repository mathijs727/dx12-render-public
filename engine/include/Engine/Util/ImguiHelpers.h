#pragma once
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <cppitertools/enumerate.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <magic_enum/magic_enum.hpp>
DISABLE_WARNINGS_POP()
#include <array>
#include <span>
#include <tuple>
#include <type_traits>

namespace Util {

template <typename Enum>
inline std::enable_if_t<std::is_enum_v<Enum>, bool> imguiCombo(const char* label, Enum& e)
{
    static std::vector<std::string> names = []() {
        std::vector<std::string> out;
        for (const auto& name : magic_enum::enum_names<Enum>())
            out.emplace_back(name);
        return out;
    }();

    int v = (int)magic_enum::enum_index(e).value_or(0);
    const bool changed = ImGui::Combo(
        label, &v,
        [](void* pNames, int v) -> const char* { return ((const std::string*)pNames)[v].c_str(); },
        names.data(), (int)names.size());
    e = magic_enum::enum_value<Enum>(v);
    return changed;
}

// NOTE(Mathijs): ID is used to ensure that the static variable doesn't clash with other options with the same typename T.
// Use as follows:
// imguiCombo<OptionType, struct UniqueComboName>(...);
template <typename ID, typename T, size_t N>
inline T imguiCombo(const char* pComboName, std::array<std::pair<const char*, T>, N> options)
{
    // https://github.com/ocornut/imgui/issues/1658
    static const char* pSelectedName = std::get<0>(options[0]);
    static size_t selectedIndex { 0 };
    if (ImGui::BeginCombo(pComboName, pSelectedName)) {
        for (const auto& [i, option] : iter::enumerate(options)) {
            const auto& [pName, value] = option;
            bool isSelected = (pName == pSelectedName);

            if (ImGui::Selectable(pName, isSelected)) {
                pSelectedName = pName;
                selectedIndex = i;
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    return std::get<1>(options[selectedIndex]);
}

// Use as follows:
// imguiCombo(
//     "name",
//     std::array {
//         std::pair { "Option 1", value1 },
//         std::pair { "Option 2", value2 },
//     },
//     outValue);
template <typename T, size_t N>
inline void imguiCombo(const char* pComboName, std::array<std::pair<const char*, T>, N> options, T& outValue)
{
    // https://github.com/ocornut/imgui/issues/1658
    auto selectedIter = std::find_if(
        std::begin(options), std::end(options),
        [=](const auto& lhs) {
            return std::get<T>(lhs) == outValue;
        });
    assert(selectedIter != std::end(options));
    const char* pSelectedName = std::get<const char*>(*selectedIter);
    if (ImGui::BeginCombo(pComboName, pSelectedName)) {
        for (const auto& [i, option] : iter::enumerate(options)) {
            const auto& [pName, value] = option;
            const bool isSelected = (pName == pSelectedName);
            if (isSelected)
                ImGui::SetItemDefaultFocus();

            if (ImGui::Selectable(pName, isSelected)) {
                outValue = std::get<T>(options[i]);
            }
        }
        ImGui::EndCombo();
    }
}

// NOTE(Mathijs): ID is used to ensure that the static variable doesn't clash with other options with the same typename T.
// Use as follows:
// imguiCombo<OptionType, struct UniqueComboName>(...);
template <typename ID>
inline int imguiCombo(const char* pComboName, std::span<const char* const> optionNames)
{
    static int currentIdx;
    ImGui::Combo(pComboName, &currentIdx, optionNames.data(), static_cast<int>(optionNames.size()));
    return currentIdx;
}
template <typename ID>
inline bool imguiCombo(const char* pComboName, std::span<char const* const> optionNames, int& outIdx)
{
    static int currentIdx = outIdx;
    const bool changed = ImGui::Combo(pComboName, &currentIdx, optionNames.data(), static_cast<int>(optionNames.size()));
    outIdx = currentIdx;
    return changed;
}

// NOTE(Mathijs): ID is used to ensure that the static variable doesn't clash with other options with the same typename T.
// Use as follows:
// imguiCombo<OptionType, struct UniqueComboName>(...);
template <typename ID>
inline int imguiCombo(const char* pComboName, std::span<const std::string> optionNames)
{
    std::vector<const char*> optionNamePointers;
    for (const auto& optionName : optionNames)
        optionNamePointers.push_back(optionName.c_str());

    static int currentIdx;
    ImGui::Combo(pComboName, &currentIdx, optionNamePointers.data(), static_cast<int>(optionNamePointers.size()));
    return currentIdx;
}

// COPIED FROM: https://gist.github.com/PossiblyAShrub/0aea9511b84c34e191eaa90dd7225969
// Creates a full screen ImGui window inside the main viewport and draws a dockspace inside it.
// Use the callback function to configure the dockspace (ImGui::DockBuilder...);
template <typename F>
inline void imguiDockingLayout(ImGuiWindowFlags windowFlags, F&& callback)
{
    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    windowFlags |= ImGuiWindowFlags_NoDocking;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    windowFlags |= ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        windowFlags |= ImGuiWindowFlags_NoBackground;

    // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
    // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
    // all active windows docked into it will lose their parent and become undocked.
    // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
    // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    ImGuiID dockSpaceID = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockSpaceID, ImVec2(0, 0), dockspace_flags);
    static auto firstTime = true;
    if (firstTime) {
        firstTime = false;

        ImGui::DockBuilderRemoveNode(dockSpaceID); // Clear existing layout.
        ImGui::DockBuilderAddNode(dockSpaceID, ImGuiDockNodeFlags_None); // Add empty root node

        callback(dockSpaceID);

        ImGui::DockBuilderFinish(dockSpaceID);
    }
    ImGui::End();
}

// Waiting for this to get released into vcpkg:
// https://github.com/ocornut/imgui/issues/211#issuecomment-857704649
inline void ImGuiPushDisabled()
{
    ImGuiContext& g = *GImGui;
    if ((g.CurrentItemFlags & ImGuiItemFlags_Disabled) == 0)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g.Style.Alpha * 0.6f);
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
}

inline void ImGuiPopDisabled()
{
    ImGuiContext& g = *GImGui;
    ImGui::PopItemFlag();
    if ((g.CurrentItemFlags & ImGuiItemFlags_Disabled) == 0)
        ImGui::PopStyleVar();
}

}
