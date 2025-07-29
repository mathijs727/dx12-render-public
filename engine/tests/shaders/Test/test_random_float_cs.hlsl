#include "Engine/Util/random.hlsl"
#include "TestShaderInputs/inputgroups/TestComputeLayout/TestRandomFloat.hlsl"

[numthreads(128, 1, 1)] void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    const uint stream = hash(dispatchThreadID.x);
    RNG rng = createRandomNumberGenerator(g_testRandomFloat.getSeed(), stream);

    RWStructuredBuffer<float> outBuffer = g_testRandomFloat.getOut();
    if (dispatchThreadID.x == 0)
        outBuffer[0] = 42.0f;
    else
        outBuffer[dispatchThreadID.x] = rng.generateFloat();
}