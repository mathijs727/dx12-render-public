#include "Engine/Render/RenderPasses/Util/ImguiPass.h"
#include "Engine/Render/FrameGraph/FrameGraphRegistry.h"
#include "Engine/Render/RenderContext.h"
#include "Engine/Render/Scene.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec2.hpp>
#include <glm/vector_relational.hpp>
#include <imgui.h>
#include <imgui_impl_dx12.h>
DISABLE_WARNINGS_POP()

namespace Render {

void ImGuiViewportPass::execute(const FrameGraphRegistry<ImGuiViewportPass>& registry, const FrameGraphExecuteArgs& args)
{
    // https://www.youtube.com/watch?v=xiSW4UgjLKU&t=877s
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin(settings.pTitle)) {
        ImVec2 viewportMin = ImGui::GetWindowContentRegionMin() + ImGui::GetCursorScreenPos();
        ImVec2 viewportMax = ImGui::GetWindowContentRegionMax() + ImGui::GetCursorScreenPos();
        const glm::ivec2 viewportSize { viewportMax.x - viewportMin.x, viewportMax.y - viewportMin.y };
        if (glm::all(glm::greaterThanEqual(viewportSize, glm::ivec2(8))) && *settings.pSize != glm::uvec2(viewportSize)) {
            *settings.pSize = viewportSize;
            *settings.pSizeChanged = true;
        }

        if (settings.pScene) {
            auto cursorPos = ImGui::GetMousePos() - ImGui::GetCursorScreenPos();
            settings.pScene->mouseCursorPosition = glm::ivec2(cursorPos.x, cursorPos.y);
        }

        auto& descriptorAllocator = args.pRenderContext->getCurrentCbvSrvUavDescriptorTransientAllocator();
        const auto descriptorAllocation = descriptorAllocator.allocate(1);
        const auto srvDesc = registry.getTextureSRV<"viewport">();
        args.pRenderContext->pDevice->CreateShaderResourceView(srvDesc.pResource, &srvDesc.desc, descriptorAllocation.firstCPUDescriptor);
        ImGui::Image((ImTextureID)descriptorAllocation.firstGPUDescriptor.ptr, ImGui::GetContentRegionAvail());
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void ImGuiPass::execute(const FrameGraphRegistry<ImGuiPass>&, const FrameGraphExecuteArgs& args)
{
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), args.pCommandList);
}

}
