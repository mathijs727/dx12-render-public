#include "ShaderInputs/inputgroups/ComputeLayout/RandomDebug.hlsl"
#include "Engine/Util/random.hlsl"

[numthreads(8, 8, 1)] void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    const uint stream = hash(dispatchThreadID.y * g_randomDebug.getTextureResolution().x + dispatchThreadID.x);
    RNG rng = createRandomNumberGenerator(g_randomDebug.getSeed(), stream);

    const float v = rng.generateFloat();
    RWTexture2D<float4> outTexture = g_randomDebug.getOutTexture();
    outTexture[dispatchThreadID.xy] = float4(v, v, v, 1.0f);
}