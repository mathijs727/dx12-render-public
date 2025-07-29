#include "pch.h"
#include <Engine/Render/RenderContext.h>

using namespace Catch::literals;

TEST_CASE("Render::RenderContext::Constructor (headless)", "[Render]")
{
    Render::RenderContext renderContext {};
    renderContext.waitForIdle();
}

TEST_CASE("Render::RenderContext::createBufferWithData", "[Render]")
{
    // Initialize Direct3D and create a RenderContext instance
    Render::RenderContext renderContext {};

    // Create a buffer with data.
    float data = 1.0f; // Create a single float initialized to 1.0f
    float readbackData = 0.0f; // Buffer to read back data

    // Create the buffer in the GPU memory.
    auto buffer = renderContext.createBufferWithData(data, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_SOURCE);

    // Copy the data from the GPU to the CPU readback buffer.
    renderContext.copyBufferFromGPUToCPU<float>(buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, std::span(&readbackData, 1));

    // Verify that the readback data matches the original data.
    REQUIRE(readbackData == data);
}

TEST_CASE("Render::RenderContext::createBufferWithArrayData", "[Render]")
{
    // Initialize Direct3D and create a RenderContext instance
    Render::RenderContext renderContext {};

    // Create a buffer with data.
    std::vector<float> data(1024, 1.0f); // Create a buffer with 1024 floats initialized to 1.0f
    std::vector<float> readbackData(1024, 0.0f); // Buffer to read back data

    // Create the buffer in the GPU memory.
    auto buffer = renderContext.createBufferWithArrayData<float>(data, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_SOURCE);

    // Copy the data from the GPU to the CPU readback buffer.
    REQUIRE(readbackData.size() == data.size());
    renderContext.copyBufferFromGPUToCPU<float>(buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, readbackData);

    // Verify that the readback data matches the original data.
    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(readbackData[i] == data[i]);
    }
}

TEST_CASE("Render::RenderContext::createBufferSRVWithArrayData", "[Render]")
{
    // Initialize Direct3D and create a RenderContext instance
    Render::RenderContext renderContext {};

    // Create a buffer with data.
    std::vector<float> data(1024, 1.0f); // Create a buffer with 1024 floats initialized to 1.0f
    std::vector<float> readbackData(1024, 0.0f); // Buffer to read back data

    // Create the buffer in the GPU memory.
    auto bufferSRV = renderContext.createBufferSRVWithArrayData<float>(data, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_SOURCE);

    // Copy the data from the GPU to the CPU readback buffer.
    REQUIRE(readbackData.size() == data.size());
    renderContext.copyBufferFromGPUToCPU<float>(bufferSRV.pResource, D3D12_RESOURCE_STATE_COPY_SOURCE, readbackData);

    // Verify that the readback data matches the original data.
    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(readbackData[i] == data[i]);
    }
}
