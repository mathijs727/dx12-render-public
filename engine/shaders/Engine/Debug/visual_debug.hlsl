#include "Engine/Util/maths.hlsl"
#include "ShaderInputs/groups/VisualDebug.hlsl"

// Constants that describe how to draw the arrow.
// CArrowInstance can be used in a ConstantBuffer.
// ArrowInstance contains padding to match the HLSL constant packing rules.
struct ArrowInstance {
    float4x4 orientation;
    float3 position;
    float length;
    float3 color;
};

struct D3D12_DRAW_INDEXED_ARGUMENTS {
    uint32_t IndexCountPerInstance;
    uint32_t InstanceCount;
    uint32_t StartIndexLocation;
    int32_t BaseVertexLocation;
    uint32_t StartInstanceLocation;
};
struct DrawCommand {
    uint64_t cbv;
    D3D12_DRAW_INDEXED_ARGUMENTS drawIndexed;
};

void visualDebugDrawArrow(VisualDebug visualDebug, float3 from, float3 to, float3 color = float3(1, 0, 1)) {
    const uint32_t CommandCountAddress = 0;
    const uint32_t CommandStartAddress = sizeof(uint64_t); // Aligned to uint64_t to prevent alignment issues with DrawCommand.
    const uint DrawConstantsAlignedSize = 256;

    if (visualDebug.paused)
        return;

    uint commandIndex;
    visualDebug.commandBuffer.InterlockedAdd(CommandCountAddress, 1, commandIndex);
    const uint constantsStart = commandIndex * DrawConstantsAlignedSize;
    const uint commandStart = CommandStartAddress + commandIndex * sizeof(DrawCommand);

    ArrowInstance arrowInstance;
    arrowInstance.orientation = make_orthonal_basis(normalize(to - from));
    arrowInstance.position = from;
    arrowInstance.length = length(to - from);
    arrowInstance.color = color;
    visualDebug.constantsBuffer.Store(constantsStart, arrowInstance);

    DrawCommand drawCommand;
    drawCommand.cbv = visualDebug.constantsBufferAddress + constantsStart;
    drawCommand.drawIndexed.IndexCountPerInstance = 576; // ???
    drawCommand.drawIndexed.InstanceCount = 1;
    drawCommand.drawIndexed.StartIndexLocation = 0;
    drawCommand.drawIndexed.BaseVertexLocation = 0;
    drawCommand.drawIndexed.StartInstanceLocation = 0;
    visualDebug.commandBuffer.Store(commandStart, drawCommand);
}
