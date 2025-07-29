#include "ShaderInputs/inputlayouts/DefaultLayout.hlsl"
#include "ShaderInputs/inputgroups/DefaultLayout/VisualDebugCamera.hlsl"
#include "Engine/Shared/vertex.hlsl"
#include "Engine/Debug/visual_debug.hlsl"

struct VS_OUTPUT {
    float4 pixelPosition : SV_Position;
    float3 worldPosition : POSITION0;
    float3 normal : NORMAL;
};

cbuffer ModelConstants : ROOT_CBV_visualDebugCBV {
    ArrowInstance arrow;
}

VS_OUTPUT main(const VS_INPUT v)
{
    float3 stretchedArrowPos = v.position;
    if (stretchedArrowPos.y > 0.5f)
        stretchedArrowPos.y += arrow.length - 1; // Translate the arrow head rather than scaling; this ensures the normals don't change (which would require scary maths).
    const float3 worldPos = mul(arrow.orientation, float4(stretchedArrowPos, 0)).xyz + arrow.position;

    VS_OUTPUT ret;
    ret.pixelPosition = mul(g_visualDebugCamera.getViewProjectionMatrix(), float4(worldPos, 1.0f));
    ret.worldPosition = worldPos;
    ret.normal = mul(arrow.orientation, float4(v.normal, 0)).xyz;
    return ret;
}
