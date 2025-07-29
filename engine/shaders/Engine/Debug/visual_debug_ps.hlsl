#include "ShaderInputs/inputgroups/DefaultLayout/VisualDebugCamera.hlsl"
#include "ShaderInputs/inputlayouts/DefaultLayout.hlsl"
#include "Engine/Debug/visual_debug.hlsl"

cbuffer ModelConstants : ROOT_CBV_visualDebugCBV {
    ArrowInstance arrow;
}

struct VS_OUTPUT {
    float4 pixelPosition : SV_Position;
    float3 worldPosition : POSITION0;
    float3 normal : NORMAL;
};

float4 main(const VS_OUTPUT fragment) : SV_TARGET
{
    const float3 V = normalize(g_visualDebugCamera.getCameraPosition() - fragment.worldPosition);
    const float3 N = fragment.normal;
    const float3 kd = arrow.color;
    return float4(kd * dot(N, V), 1.0f);
}
