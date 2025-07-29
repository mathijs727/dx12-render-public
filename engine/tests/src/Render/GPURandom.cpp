#include "GPU.h"
#include "ShaderInputs/inputgroups/TestRandomFloat.h"
#include "ShaderInputs/inputgroups/TestRandomUint.h"
#include "ShaderInputs/inputlayouts/TestComputeLayout.h"
#include "pch.h"
#include <Engine/Render/RenderContext.h>
#include <algorithm> // std::clamp

using namespace Catch::literals;

TEST_CASE("Render::GPU::Random (uint)", "[Render][GPU]")
{
    constexpr unsigned bufferSize = 1024;
    constexpr unsigned workgroupSize = 128;

    Render::RenderContext renderContext {};
    auto pCommandList = renderContext.commandListManager.acquireCommandList();

    // Load the shader and create a pipeline state object.
    auto pipelineState = createComputePipeline(renderContext, "Test/test_random_uint_cs.dxil");

    // Create output buffer.
    const auto& outputBuffer = createUAVBuffer<uint32_t>(renderContext, pCommandList, bufferSize);

    // Set up the inputs to the shader.
    ShaderInputs::TestRandomUint inputs {};
    inputs.setSeed(10108870980642855433ull);
    inputs.setOut(outputBuffer);
    const auto compiledInputs = inputs.generateTransientBindings(renderContext);

    // Record the command list to execute the shader.
    setPipelineState(pCommandList, pipelineState);
    setDescriptorHeaps(pCommandList, renderContext);
    ShaderInputs::TestComputeLayout::bindMainCompute(pCommandList.Get(), compiledInputs);
    pCommandList->Dispatch(bufferSize / workgroupSize, 1, 1);

    // Execute the shader.
    renderContext.getCurrentCbvSrvUavDescriptorTransientAllocator().flush();
    renderContext.submitGraphicsQueue(pCommandList);
    renderContext.waitForIdle();

    std::vector<uint32_t> readbackData(bufferSize, 0);
    renderContext.copyBufferFromGPUToCPU<uint32_t>(outputBuffer.pResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, readbackData);

    // Verify the output data.
    REQUIRE(readbackData[0] == 42); // First thread writes [42] to ensure the shader is running.

    std::vector<uint32_t> bitHistogram(32, 0);
    for (size_t i = 1; i < readbackData.size(); ++i) {
        const auto value = readbackData[i];
        for (size_t bit = 0; bit < 32; ++bit) {
            if (value & (1u << bit)) {
                ++bitHistogram[bit];
            }
        }
    }

    // Verify that the bits are uniformly distributed.
    for (size_t bit = 0; bit < 32; ++bit) {
        // Allow a small margin of error due to randomness.
        REQUIRE(bitHistogram[bit] >= 384);
        REQUIRE(bitHistogram[bit] <= 640);
    }
}

TEST_CASE("Render::GPU::Random (float)", "[Render][GPU]")
{
    constexpr unsigned bufferSize = 1024;
    constexpr unsigned workgroupSize = 128;

    Render::RenderContext renderContext {};
    auto pCommandList = renderContext.commandListManager.acquireCommandList();

    // Load the shader and create a pipeline state object.
    auto pipelineState = createComputePipeline(renderContext, "Test/test_random_float_cs.dxil");

    // Create output buffer.
    const auto& outputBuffer = createUAVBuffer<float>(renderContext, pCommandList, bufferSize);

    // Set up the inputs to the shader.
    ShaderInputs::TestRandomFloat inputs {};
    inputs.setSeed(10108870980642855433ull);
    inputs.setOut(outputBuffer);
    const auto compiledInputs = inputs.generateTransientBindings(renderContext);

    // Record the command list to execute the shader.
    setPipelineState(pCommandList, pipelineState);
    setDescriptorHeaps(pCommandList, renderContext);
    ShaderInputs::TestComputeLayout::bindMainCompute(pCommandList.Get(), compiledInputs);
    pCommandList->Dispatch(bufferSize / workgroupSize, 1, 1);

    // Execute the shader.
    renderContext.getCurrentCbvSrvUavDescriptorTransientAllocator().flush();
    renderContext.submitGraphicsQueue(pCommandList);
    renderContext.waitForIdle();

    std::vector<float> readbackData(bufferSize, 0.0f);
    renderContext.copyBufferFromGPUToCPU<float>(outputBuffer.pResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, readbackData);

    // Verify the output data.
    REQUIRE(readbackData[0] == Catch::Approx(42.0f)); // First thread writes [42] to ensure the shader is running.

    std::vector<uint32_t> histogram(8, 0);
    for (size_t i = 1; i < readbackData.size(); ++i) {
        histogram[std::clamp(int(readbackData[i] * 8), 0, 7)]++; // Count the last 3 bits.
    }
    // Verify that the values are uniformly distributed.
    // 1023 values divided over 8 bins; expect each bin to have around 128 values.
    for (size_t bin = 0; bin < 8; ++bin) {
        // Allow a margin of error due to randomness.
        REQUIRE(histogram[bin] >= 96);
        REQUIRE(histogram[bin] <= 160);
    }
}