#include "Engine/Util/random.hlsl"
#include "TestShaderInputs/inputgroups/TestComputeLayout/TestRandomUint.hlsl"

[numthreads(128, 1, 1)] void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    const uint stream = hash(dispatchThreadID.x);
    RNG rng = createRandomNumberGenerator(g_testRandomUint.getSeed(), stream);

    RWStructuredBuffer<uint> outBuffer = g_testRandomUint.getOut();
    if (dispatchThreadID.x == 0)
        outBuffer[0] = 42;
    else
        outBuffer[dispatchThreadID.x] = rng.generate();
}