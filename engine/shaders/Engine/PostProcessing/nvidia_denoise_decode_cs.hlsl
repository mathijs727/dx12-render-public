#include "ShaderInputs/inputgroups/ComputeLayout/NvidiaDenoiseDecode.hlsl"
// NVIDIA Real-Time Denoiser
#include "NRDEncoding.hlsli"
#include "NRD.hlsli"

[numthreads(8, 8, 1)] void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    Texture2D<float4> inTexture = g_nvidiaDenoiseDecode.getInTexture();
    RWTexture2D<float4> outTexture = g_nvidiaDenoiseDecode.getOutTexture();
    const float4 packedData = inTexture[dispatchThreadID.xy];
    const float4 unpackedData = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(packedData);
    outTexture[dispatchThreadID.xy] = float4(unpackedData.xyz, 1.0f);
}