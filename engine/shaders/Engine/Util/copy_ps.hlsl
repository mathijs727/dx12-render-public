#include "ShaderInputs/inputgroups/DefaultLayout/CopyRaster.hlsl"
#include "full_screen_vertex.hlsl"

float4 main(FULL_SCREEN_VS_OUTPUT vertex)
    : SV_TARGET
{
    return g_copyRaster.getInTexture().Load(int3(vertex.position.xy, 0));
    //return float4(1, 1, 1, 1);
}