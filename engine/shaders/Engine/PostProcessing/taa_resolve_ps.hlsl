#include "ShaderInputs/inputgroups/DefaultLayout/TAAResolve.hlsl"
#include "ShaderInputs/inputlayouts/DefaultLayout.hlsl"
#include "Engine/Util/full_screen_vertex.hlsl"

float3 reprojectPixel(in float2 normalizedPixel, in float depth, in float4x4 invViewProjectionMatrix) {
    float2 pixelCoordinates =  normalizedPixel * 2.0f - 1.0f; // [-1, +1]
    pixelCoordinates.y = -pixelCoordinates.y;
    float4 tmp = mul(invViewProjectionMatrix, float4(pixelCoordinates, depth, 1));
    return tmp.xyz / tmp.w;
}

float length2(in float3 lhs) {
    return dot(lhs, lhs);
}
float length2(in float2 lhs) {
    return dot(lhs, lhs);
}

float3 rgbToYCoCg(in float3 rgb) {
    // https://en.wikipedia.org/wiki/YCoCg#Conversion_with_the_RGB_color_model
    return float3(
        0.25 * rgb.r + 0.5 * rgb.g + 0.25 * rgb.b,
        0.5 * rgb.r + 0.0 * rgb.g - 0.5 * rgb.b,
        -0.25 * rgb.r + 0.5 * rgb.g - 0.25 * rgb.b
    );
}
float3 yCoCgToRgb(in float3 yCoCg) {
    // https://en.wikipedia.org/wiki/YCoCg#Conversion_with_the_RGB_color_model
    return float3(
        yCoCg.r + yCoCg.g - yCoCg.b,
        yCoCg.r + yCoCg.b,
        yCoCg.r - yCoCg.g - yCoCg.b
    );
}

// COPIED FROM:
// https://www.shadertoy.com/view/MllSzX
float3 CubicHermite(float3 A, float3 B, float3 C, float3 D, float t)
{
	const float t2 = t*t;
    const float t3 = t*t*t;
    const float3 a = -A/2.0 + (3.0*B)/2.0 - (3.0*C)/2.0 + D/2.0;
    const float3 b = A - (5.0*B)/2.0 + 2.0*C - D / 2.0;
    const float3 c = -A/2.0 + C/2.0;
   	const float3 d = B;
    return a*t3 + b*t2 + c*t + d;
}

// COPIED FROM:
// https://www.shadertoy.com/view/MllSzX
float3 BicubicHermiteTextureSample(in Texture2D texture, in float2 P, in float2 textureResolution)
{
    float2 pixel = P * textureResolution + 0.5;
    const float2 onePixel = 1.0f / textureResolution;
    const float2 twoPixels = 2.0f / textureResolution;

    const float2 fraction = float2(frac(pixel.x), frac(pixel.y));
    pixel = floor(pixel) / textureResolution - float2(onePixel / 2.0);
    
#define _sampler g_taaNNSampler
    const float3 C00 = texture.SampleLevel(_sampler, pixel + float2(-onePixel.x ,-onePixel.y), 0).rgb;
    const float3 C10 = texture.SampleLevel(_sampler, pixel + float2( 0.0        ,-onePixel.y), 0).rgb;
    const float3 C20 = texture.SampleLevel(_sampler, pixel + float2( onePixel.x ,-onePixel.y), 0).rgb;
    const float3 C30 = texture.SampleLevel(_sampler, pixel + float2( twoPixels.x,-onePixel.y), 0).rgb;
    
    const float3 C01 = texture.SampleLevel(_sampler, pixel + float2(-onePixel.x , 0.0), 0).rgb;
    const float3 C11 = texture.SampleLevel(_sampler, pixel + float2( 0.0        , 0.0), 0).rgb;
    const float3 C21 = texture.SampleLevel(_sampler, pixel + float2( onePixel.x , 0.0), 0).rgb;
    const float3 C31 = texture.SampleLevel(_sampler, pixel + float2( twoPixels.x, 0.0), 0).rgb;
    
    const float3 C02 = texture.SampleLevel(_sampler, pixel + float2(-onePixel.x , onePixel.y), 0).rgb;
    const float3 C12 = texture.SampleLevel(_sampler, pixel + float2( 0.0        , onePixel.y), 0).rgb;
    const float3 C22 = texture.SampleLevel(_sampler, pixel + float2( onePixel.x , onePixel.y), 0).rgb;
    const float3 C32 = texture.SampleLevel(_sampler, pixel + float2( twoPixels.x, onePixel.y), 0).rgb;
    
    const float3 C03 = texture.SampleLevel(_sampler, pixel + float2(-onePixel.x , twoPixels.y), 0).rgb;
    const float3 C13 = texture.SampleLevel(_sampler, pixel + float2( 0.0        , twoPixels.y), 0).rgb;
    const float3 C23 = texture.SampleLevel(_sampler, pixel + float2( onePixel.x , twoPixels.y), 0).rgb;
    const float3 C33 = texture.SampleLevel(_sampler, pixel + float2( twoPixels.x, twoPixels.y), 0).rgb;
#undef _sampler
    
    const float3 CP0X = CubicHermite(C00, C10, C20, C30, fraction.x);
    const float3 CP1X = CubicHermite(C01, C11, C21, C31, fraction.x);
    const float3 CP2X = CubicHermite(C02, C12, C22, C32, fraction.x);
    const float3 CP3X = CubicHermite(C03, C13, C23, C33, fraction.x);
    return CubicHermite(CP0X, CP1X, CP2X, CP3X, fraction.y);
}

// Temporal Anti Aliasing (TAA)
// https://developer.download.nvidia.com/gameworks/events/GDC2016/msalvi_temporal_supersampling.pdf
// http://behindthepixels.io/assets/files/TemporalAA.pdf
// https://www.elopezr.com/temporal-aa-and-the-quest-for-the-holy-trail/
float4 main(FULL_SCREEN_VS_OUTPUT vertex)
    : SV_TARGET
{
    const int3 pixelI3 = int3(vertex.position.xy, 0);
    const float3 newColorRGB = g_tAAResolve.getFrameBuffer().Load(pixelI3).xyz;
#if 0
    const float2 velocity = g_tAAResolve.getVelocity().Load(pixelI3);
#else
    // Pick longest motion vector inside 1-ring neighbours.
    float2 velocity = float2(0, 0);
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0)
                continue;

            const int3 neighbourI3 = pixelI3 + int3(dx, dy, 0);
            const float2 neighbourVelocity = g_tAAResolve.getVelocity().Load(neighbourI3);
            if (length2(neighbourVelocity) > length2(velocity))
                velocity = neighbourVelocity;
        }
    }
#endif
    //const float2 reprojectedPixel = vertex.position.xy - velocity;
    const float2 normalizedPixel = ((vertex.position.xy + 0.0f) / g_tAAResolve.getResolution()); // [0, 1]
    const float2 normalizedReprojectedPixel = normalizedPixel - 0.5f * float2(velocity.x, -velocity.y); // [0, 1]
    float2 normalizedHalfPixel = 0.0f / g_tAAResolve.getResolution();
    normalizedHalfPixel.y = -normalizedHalfPixel.y;
    //float3 historyColorRGB = g_tAAResolve.getHistory().SampleLevel(g_taaSampler, normalizedReprojectedPixel + normalizedHalfPixel, 0).xyz;
    float3 historyColorRGB = BicubicHermiteTextureSample(g_tAAResolve.getHistory(), normalizedReprojectedPixel, g_tAAResolve.getResolution()).xyz;
    //const float3 history = g_tAAResolve.getHistory().Load(int3(reprojectedPixel, 0)).xyz;
    
#if 1
    // === Color clamping inspired by Unreal Engine 4 ===
    // Desired goal: clamp the color to within the convex hull of the color values of neighbors of the current frame.
    // Computing the convex hull is complicated (especially in shaders) thus we resort to a simple AABB as approximation.
    // The AABB is computed in the yCoCg color space to create tighter bounds.
    // 
    // Source:
    // http://behindthepixels.io/assets/files/TemporalAA.pdf
    // https://www.elopezr.com/temporal-aa-and-the-quest-for-the-holy-trail/
    float3 minColorBoundsYCoCg = float3(100000, 100000, 100000), maxColorBoundsYCoCg = float3(-100000, -100000, -100000);
    float3 m1 = float3(0, 0, 0), m2 = float3(0, 0, 0);
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int3 neighbourI3 = pixelI3 + int3(dx, dy, 0);
            const float3 neighborColorRGB = g_tAAResolve.getFrameBuffer().Load(neighbourI3).xyz;
            const float3 neighborColorYCoCg = rgbToYCoCg(neighborColorRGB);
            minColorBoundsYCoCg = min(minColorBoundsYCoCg, neighborColorYCoCg);
            maxColorBoundsYCoCg = max(maxColorBoundsYCoCg, neighborColorYCoCg);
            m1 += neighborColorYCoCg;
            m2 += neighborColorYCoCg * neighborColorYCoCg;
        }
    }
    // Variance clipping as described in:
    // http://behindthepixels.io/assets/files/TemporalAA.pdf
    // https://developer.download.nvidia.com/gameworks/events/GDC2016/msalvi_temporal_supersampling.pdf (slide 24)
    // Mean & variance (aka 1st and 2nd color moments)
    const float N = 9.0f;
    const float3 mu = m1 / N;
    const float3 sigma = sqrt(m2 / N - mu*mu);
    // Parameter chosen between 0.75 and 1.25
    // http://behindthepixels.io/assets/files/TemporalAA.pdf (equation 6)
    const float gamma = 1.0f;
    // Clamp new bounds against the old AABB (slide 25)
    minColorBoundsYCoCg = max(minColorBoundsYCoCg, mu - gamma * sigma);
    maxColorBoundsYCoCg = min(maxColorBoundsYCoCg, mu + gamma * sigma);

    float3 histroyColorYCoCg = rgbToYCoCg(historyColorRGB);
    histroyColorYCoCg = clamp(histroyColorYCoCg , minColorBoundsYCoCg, maxColorBoundsYCoCg);
    historyColorRGB = yCoCgToRgb(histroyColorYCoCg);
#endif

    const float depth = g_tAAResolve.getDepth().Load(pixelI3);
    const float historyDepth = g_tAAResolve.getHistoryDepth().SampleLevel(g_taaSampler, normalizedReprojectedPixel, 0);
    const float3 worldPos = reprojectPixel(normalizedPixel, depth, g_tAAResolve.getInverseViewProjectionMatrix());
    const float3 previousWorldPos = reprojectPixel(normalizedReprojectedPixel, historyDepth, g_tAAResolve.getInverseViewProjectionMatrix());
    //return float4(depth/10,depth/10,depth/10,1);
    //return float4(abs(previousWorldPos) / 10.0f, 1);

    float alpha = g_tAAResolve.getAlpha();
    //const float reprojectionDistanceThreshold = length2(velocity) > 0.00001 && length2(worldPos - previousWorldPos) > 0.001f;
    const float reprojectionDistanceThreshold = length2(velocity) > 0.00001 && abs(depth - historyDepth) > 0.0005f;
    const float reprojectionOutOfBounds = any(normalizedReprojectedPixel < 0.0f | normalizedReprojectedPixel > 1.0f);
    const float reprojectionBackground = historyDepth == 1.0f;
    if (reprojectionDistanceThreshold || reprojectionOutOfBounds || reprojectionBackground) {
        /*if (reprojectionDistanceThreshold)
            return float4(1,0,0,1);
        if (reprojectionOutOfBounds)
                return float4(0,1,0,1);
        if (reprojectionBackground)
                return float4(0,0,1,1);*/
        alpha = 1.0f;
    }
    return float4(alpha * newColorRGB + (1 - alpha) * historyColorRGB, 1.0f);
}