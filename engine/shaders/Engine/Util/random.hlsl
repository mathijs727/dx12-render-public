#ifndef __RANDOM_HLSL__
#define __RANDOM_HLSL__

struct RNG {
    uint64_t state;
    uint32_t inc;

    uint generate() {
        // Advance internal state.
        state = state * 6364136223846793005ull + uint64_t(inc);

        // Calculate output function (XSH RR).
        const uint32_t xorShifted = uint32_t(((state >> 18u) ^ state) >> 27u);
        const uint32_t rot = uint32_t(state >> 59u);
        return (xorShifted >> rot) | (xorShifted << ((-rot) & 31));
    }
    float generateFloat() {
        return float(generate()) / float(0xFFFFFFFF);
    }
    float2 generateFloat2() {
        // Not sure what the rules are with respect to execution order of arguments so I'll just compute them in separate variables.
        const float x = generateFloat();
        const float y = generateFloat();
        return float2(x, y);
    }
    float3 generateFloat3() {
        // Not sure what the rules are with respect to execution order of arguments so I'll just compute them in separate variables.
        const float x = generateFloat();
        const float y = generateFloat();
        const float z = generateFloat();
        return float3(x, y, z);
    }
};

// Hash function from H. Schechter & R. Bridson, goo.gl/RXiKaH
// https://github.com/keijiro/ComputePrngTest/blob/master/Assets/Prng.compute
uint hash(uint s)
{
    s ^= 2747636419u;
    s *= 2654435769u;
    s ^= s >> 16;
    s *= 2654435769u;
    s ^= s >> 16;
    s *= 2654435769u;
    return s;
}


RNG createRandomNumberGenerator(uint64_t seed, uint stream)
{
    RNG rng;
    rng.state = 0;
    rng.inc = (stream << 1) | 1; // Must be odd => 2^31 instead of 2^32
    rng.generate();
    rng.state += seed;
    rng.generate();
    return rng;
}

RNG createRandomNumberGeneratorFromFragment(uint64_t seed, float2 uv)
{
    // Fill the lower 2 bytes, should be enough...
    const uint pixel1D = uint(uv.y * 65536.0f + uv.x);
    return createRandomNumberGenerator(seed, hash(pixel1D));
}

RNG createRandomNumberGeneratorFromUV(uint64_t seed, float2 uv)
{
    // Fill the lower 2 bytes, should be enough...
    const uint x = uint(uv.x * 65536.0f);
    const uint y = uint(uv.y * 65536.0f);
    const uint stream = x ^ y;
    return createRandomNumberGenerator(seed, stream);
}

#endif // __RANDOM_HLSL__