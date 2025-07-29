#ifndef __VERTEX_HLSL__
#define __VERTEX_HLSL__

struct VS_INPUT {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

#endif // __VERTEX_HLSL__
