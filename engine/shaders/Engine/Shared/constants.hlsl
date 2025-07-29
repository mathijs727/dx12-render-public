#ifndef __CONSTANTS_HLSL__
#define __CONSTANTS_HLSL__

static const float PI = 3.14159265f;
static const float ONE_OVER_PI = 1.0f / PI;
static const float ONE_OVER_TWO_PI = 0.5f / PI;

// Colors
static const float3 BLACK = float3(0, 0, 0);
static const float3 WHITE = float3(1, 1, 1);
static const float3 RED = float3(1, 0, 0);
static const float3 GREEN = float3(0, 1, 0);
static const float3 BLUE = float3(0, 0, 1);

// Ray tracing constants
static const float RAY_EPSILON = 0.001f; // Small offset to avoid self-intersection

#endif // __CONSTANTS_HLSL__
