#include "Engine/Util/full_screen_vertex.hlsl"
#include "Engine/Shared/vs_output.hlsl"

struct VERTEX_DATA {
    float4 position : SV_Position;
    float3 normal : NORMAL0;
    float3 color : COLOR0;
};

float4 main(const VERTEX_DATA fragment) : SV_TARGET
{
    const float3 normalAsColor = 0.5 * fragment.normal + 0.5;
    const float3 outColor = fragment.color * normalAsColor;
    return float4(outColor, 1);
}
