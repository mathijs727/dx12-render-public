#include "common.hlsl"
#include "ShaderInputs/inputgroups/RayTraceGlobalLayout/RayTracePipelineDebug.hlsl"

[shader("miss")]
void miss(inout HitInfo payload) {
    payload.colorAndDistance = float4(1, 0, 0, 1);
}