#include "ShaderInputs/inputgroups/DefaultLayout/ColorCorrection.hlsl"
#include "Engine/Util/full_screen_vertex.hlsl"
#include "ACES.hlsl"

// Copied from course notes "Moving Frostbite to PBR" by EA / Dice
// https://www.ea.com/frostbite/news/moving-frostbite-to-pb
// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
float3 approximationSRgbToLinear(in float3 sRGBCol)
{
    return pow(sRGBCol, 2.2f);
}

float3 approximationLinearToSRGB(in float3 linearCol)
{
    return pow(linearCol, 1.0f / 2.2f);
}
 
float3 accurateSRGBToLinear(in float3 sRGBCol)
{
    float3 linearRGBLo = sRGBCol / 12.92f;
    float3 linearRGBHi = pow((sRGBCol + 0.055f) / 1.055f, 2.4f);
    float3 linearRGB = select(sRGBCol <= 0.04045f, linearRGBLo, linearRGBHi);
    return linearRGB;
}
 
float3 accurateLinearToSRGB(in float3 linearCol)
{
    float3 sRGBLo = linearCol * 12.92f;
    float3 sRGBHi = (pow(abs(linearCol), 1.0f/2.4f) * 1.055f) - 0.055f;
    float3 sRGB = select(linearCol <= 0.0031308f, sRGBLo, sRGBHi);
    return sRGB;
}
  
// Copied from:
// https://github.com/Zackin5/Filmic-Tonemapping-ReShade/blob/master/Uncharted2.fx
static float U2_W = 11.2f; // White point value
float3 uncharted2Tonemap(float3 x)
{
    const float U2_A = 0.22f; // Shoulder strength
    const float U2_B = 0.30f; // Linear strength
    const float U2_C = 0.10f; // Linear angle
    const float U2_D = 0.20f; // Toe strength
    const float U2_E = 0.01f; // Toe numerator
    const float U2_F = 0.30f; // Toe demonimator
    return ((x * (U2_A * x + U2_C * U2_B) + U2_D * U2_E) / (x * (U2_A * x + U2_B) + U2_D * U2_F)) - U2_E / U2_F;
}

float3 ACESFilm(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}

// https://www.shadertoy.com/view/dtyfRw
static float3x3 SRGB_2_XYZ_MAT = float3x3(
    0.4124564, 0.3575761, 0.1804375,
    0.2126729, 0.7151522, 0.0721750,
    0.0193339, 0.1191920, 0.9503041);
float luminance(float3 color) {
    float3 luminanceCoefficients = SRGB_2_XYZ_MAT[1];
    return dot(color, luminanceCoefficients);
}
static float3x3 agxTransform = float3x3(
    0.842479062253094, 0.0423282422610123, 0.0423756549057051,
    0.0784335999999992, 0.878468636469772, 0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793104);

static float3x3 agxTransformInverse = float3x3(
    1.19687900512017, -0.0528968517574562, -0.0529716355144438,
    -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
    -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
float3 agxDefaultContrastApproximation(float3 x) {
    const float3 x2 = x * x;
    const float3 x4 = x2 * x2;
    return +15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}
void agx(inout float3 color) {
    const float minEv = -12.47393;
    const float maxEv = 4.026069;
    color = mul(agxTransform, color);
    color = clamp(log2(color), minEv, maxEv);
    color = (color - minEv) / (maxEv - minEv);
    color = agxDefaultContrastApproximation(color);
}
void agxEotf(inout float3 color) {
    color = mul(agxTransformInverse, color);
}
void agxLook(inout float3 color) {
    // Punchy
    const float3 slope = float3(1.1f, 1.1f, 1.1f);
    const float3 power = float3(1.2f, 1.2f, 1.2f);
    const float saturation = 1.3f;
    float luma = luminance(color);
    color = pow(color * slope, power);
    color = max(luma + saturation * (color - luma), float3(0.0f, 0.0f, 0.0f));
}

float4 main(FULL_SCREEN_VS_OUTPUT vertex)
    : SV_TARGET
{
    float3 linearColor = g_colorCorrection.getLinearFrameBuffer().Load(int3(vertex.position.xy, 0)).xyz;
    linearColor *= g_colorCorrection.getInvSampleCount();
    const int toneMapping = g_colorCorrection.getToneMappingFunction();

    float3 srgbColor;
    if (toneMapping == 1) {
        linearColor = uncharted2Tonemap(linearColor);
        // https://github.com/Zackin5/Filmic-Tonemapping-ReShade/blob/master/Uncharted2.fx
        if (g_colorCorrection.getEnableWhitePoint())
            linearColor /= uncharted2Tonemap(U2_W);
    } else if (toneMapping == 2) {
        //linearColor = accurateSRGBToLinear(ACESFitted(accurateLinearToSRGB(linearColor)));
        linearColor = ACESFilm(linearColor);
    } else if (toneMapping == 3) {
        // https://www.shadertoy.com/view/dtyfRw
        agx(linearColor);
        agxEotf(linearColor);
        agxLook(linearColor);
    }

    srgbColor = g_colorCorrection.getEnableGammaCorrection() ? accurateLinearToSRGB(linearColor) : linearColor;
    return float4(srgbColor, 1);
}