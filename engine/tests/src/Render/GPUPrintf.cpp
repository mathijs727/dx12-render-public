#include "GPU.h"
#include "ShaderInputs/inputgroups/TestPrintf.h"
#include "ShaderInputs/inputlayouts/TestComputeLayout.h"
#include "pch.h"
#include <Engine/Core/Window.h>
#include <Engine/Render/FrameGraph/FrameGraph.h>
#include <Engine/Render/FrameGraph/Operations.h>
#include <Engine/Render/RenderContext.h>
#include <Engine/Render/RenderPasses/Util/Printf.h>
#include <algorithm> // std::clamp
#include <spdlog/sinks/ostream_sink.h>
#include <sstream>

using namespace Catch::literals;
using namespace Render;

struct TestPrintfPass {
public:
    static constexpr RenderPassType renderPassType = RenderPassType::Compute;

    struct Settings {
        PrintfPass* pPrintfPass;
    } settings;

    static consteval void declareFrameResources(RenderPassBuilder& builder) { }
    inline void initialize(const RenderContext& renderContext)
    {
        m_pipelineState = ::createComputePipeline(renderContext, "Test/test_printf_cs.dxil");
    }
    inline void execute(const FrameGraphRegistry<TestPrintfPass>& registry, const FrameGraphExecuteArgs& args)
    {
        setPipelineState(args.pCommandList, m_pipelineState);
        ShaderInputs::TestPrintf inputs {};
        inputs.setPrintSink(settings.pPrintfPass->getShaderInputs());
        const auto compiledInputs = inputs.generateTransientBindings(*args.pRenderContext);
        ShaderInputs::TestComputeLayout::bindMainCompute(args.pCommandList, compiledInputs);
        args.pCommandList->Dispatch(1, 1, 1);
    }

private:
    RenderAPI::PipelineState m_pipelineState;
};

TEST_CASE("Render::GPU::Printf", "[Render][GPU]")
{
    Render::RenderContext renderContext {};
    Render::FrameGraphBuilder frameGraphBuilder { &renderContext };
    PrintfPass* pPrintfPass = frameGraphBuilder.addOperation<PrintfPass>().finalize();
    frameGraphBuilder.addOperation<TestPrintfPass>({ pPrintfPass }).finalize();

    std::ostringstream sstream;
    auto pOldDefaultLogger = spdlog::default_logger();
    auto pOstreamSink = std::make_shared<spdlog::sinks::ostream_sink_mt>(sstream);
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("ostream_capture_log", pOstreamSink));

    auto frameGraph = frameGraphBuilder.compile();
    for (uint32_t i = 0; i < 10; ++i) {
        renderContext.waitForNextFrame();
        renderContext.resetFrameAllocators();
        frameGraph.execute();
        renderContext.present();
    }
    renderContext.waitForIdle();
    spdlog::set_default_logger(pOldDefaultLogger);

    auto output = sstream.str();
    REQUIRE(output.find("SUBSTRING NOT IN OUTPUT") == std::string::npos);
    REQUIRE(output.find("Hello World = 12") != std::string::npos);
}