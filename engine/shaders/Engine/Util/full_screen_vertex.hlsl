#ifndef __FULL_SCREEN_VERTEX_HLSL__
#define __FULL_SCREEN_VERTEX_HLSL__

struct FULL_SCREEN_VS_OUTPUT {
    float2 texCoord : TEXCOORD;
    float4 position : SV_Position;
};

struct FULL_SCREEN_PS_INPUT {
    float2 texCoord : TEXCOORD;
    float4 position : SV_Position;
};

#endif // __FULL_SCREEN_VERTEX_HLSL__
