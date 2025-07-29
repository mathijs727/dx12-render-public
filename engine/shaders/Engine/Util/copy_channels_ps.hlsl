#include "ShaderInputs/inputgroups/DefaultLayout/CopyChannelRaster.hlsl"
#include "full_screen_vertex.hlsl"

float copyChannel(in float4 value, in uint32_t channel) {
    if (channel == 0)
        return value.r;
    else if (channel == 1)
        return value.g;
    else if (channel == 2)
        return value.b;
    else if (channel == 3)
        return value.a;
    else
        return 0.0f;
}

float4 main(FULL_SCREEN_VS_OUTPUT vertex)
    : SV_TARGET
{
    const float4 inPixel = g_copyChannelRaster.getInTexture().Load(int3(vertex.position.xy, 0));
    float4 outPixel = float4(0, 0, 0, 1);
    outPixel.r = copyChannel(inPixel, g_copyChannelRaster.getR());
    outPixel.g = copyChannel(inPixel, g_copyChannelRaster.getG());
    outPixel.b = copyChannel(inPixel, g_copyChannelRaster.getB());
    outPixel.rgb =  g_copyChannelRaster.getScaling() * outPixel.rgb + g_copyChannelRaster.getOffset();
    return outPixel;
}