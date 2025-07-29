#ifndef __VS_OUTPUT_HLSL__
#define __VS_OUTPUT_HLSL__

struct VS_OUTPUT {
    float4 pixelPosition : SV_Position;

    float3 worldPosition : POSITION0;
    float3 viewSpacePosition : POSITION1;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VS_OUTPUT_TAA {
    float4 jitteredPixelPosition : SV_Position;

    float3 worldPosition : POSITION0;
    float3 viewSpacePosition : POSITION1;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;

    float4 screenPosition : POSITION2;
    float4 lastFrameScreenPosition: POSITION3;
};

#endif // __VS_OUTPUT_HLSL__