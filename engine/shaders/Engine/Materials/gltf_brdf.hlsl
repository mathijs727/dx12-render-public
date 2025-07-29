#ifndef __GLTF_BRDF_HLSL__
#define __GLTF_BRDF_HLSL__
#include "Engine/Shared/constants.hlsl"
#include "Engine/Shared/surface_interaction.hlsl"

struct GLTF_BRDF {
    float3 baseColor;
    float metallic;
    float alpha; // roughness^2
    uint32_t materialID;
};

float4 __brdf_sample_tex(in const Texture2D texture, in const SamplerState sampler_, in const float2 uv) {
#if __SHADER_TARGET_STAGE  == __SHADER_STAGE_LIBRARY
    return texture.SampleLevel(sampler_, uv, 0);
#else
    return texture.Sample(sampler_, uv);
#endif
}

void brdf_sample(in float2 uv, out float3 wi, out float pdf) {
#if 0 // Uniform hemisphere sampling.
    // https://www.rorydriscoll.com/2009/01/07/better-sampling/
    const float r = sqrt(1.0f - uv.x*uv.x);
    const float phi = 2 * PI * uv.y;
    wi = float3(cos(phi) * r, sin(phi) * r, uv.x);
    pdf = ONE_OVER_TWO_PI;
#else // Cosine weighted sampling.
    const float r = sqrt(uv.x);
    const float theta = 2 * PI * uv.y;
 
    const float x = r * cos(theta);
    const float y = r * sin(theta);
 
    wi = float3(x, y, sqrt(max(0, 1 - uv.x)));
    pdf = wi.z * ONE_OVER_PI;
#endif
}

#ifdef __SinglePBRMaterial__
bool brdf_construct(
    in const SurfaceInteraction si,
    in const SinglePBRMaterial inputs,
    out GLTF_BRDF brdf)
{
    const Texture2D baseColorTexture = inputs.getBaseColorTexture();
    float4 baseColorTexSample = __brdf_sample_tex(baseColorTexture, g_materialSampler, si.uv);
    if (baseColorTexSample.w < 0.5)
        return false;

    const PBRMaterial constants = inputs.getMaterial();
    brdf.baseColor = baseColorTexSample.rgb * constants.baseColor;
    brdf.metallic = constants .metallic;
    brdf.alpha = constants.alpha;
    return true;
}

bool brdf_shouldDiscard(in const SinglePBRMaterial instanceInputs, const float2 uv)
{
    const Texture2D baseColorTexture = instanceInputs.getBaseColorTexture();
    return baseColorTexture.Sample(g_materialSampler, uv).w == 0;
}
#endif

struct Angles {
    float NdotL;
    float NdotV;
    float NdotH;
    float LdotH;
    float VdotH;
};

// Heaviside function
float Xp(float x) {
    return x > 0.0f ? 1.0f : 0.0f;
}

float specular_brdf(in float alpha, in Angles angles) {
    const float alpha2 = alpha * alpha;
    const float NdotL2 = angles.NdotL *angles.NdotL;
    const float NdotV2 = angles.NdotV *angles.NdotV;
    const float NdotH2 = angles.NdotH *angles.NdotH;

    const float D_numerator = alpha2 * Xp(angles.NdotH);
    const float D_denominator_1 = NdotH2 * (alpha2 - 1) + 1;
    const float D_denominator = PI * D_denominator_1 * D_denominator_1;
    const float D = D_numerator / D_denominator;

    const float V1_numerator = Xp(angles.LdotH);
    const float V1_denominator = abs(angles.NdotL) + sqrt(alpha2 + (1 - alpha2) * NdotL2);
    const float V2_numerator = Xp(angles.VdotH);
    const float V2_denominator = abs(angles.NdotV) + sqrt(alpha2 + (1 - alpha2) * NdotV2);
    const float V = (V1_numerator / V1_denominator) * (V2_numerator / V2_denominator);
    return V * D;
}

float3 diffuse_brdf(in float3 color) {
    return ONE_OVER_PI * color;
}

float3 conductor_fresnel(in float3 f0, in float bsdf, in Angles angles) {
    const float tmp = 1 - abs(angles.VdotH);
    const float tmp2 = tmp * tmp;
    const float tmp5 = tmp2 * tmp2 * tmp;
    return bsdf * (f0 + (1 - f0) * tmp5);
}

float3 fresnel_mix(in float ior, in float3 base, in float3 layer, in Angles angles) {
    const float f0_1 = (1 - ior) / (1 + ior);
    const float f0 = f0_1 * f0_1;
    const float tmp = 1 - abs(angles.VdotH);
    const float tmp2 = tmp * tmp;
    const float tmp5 = tmp2 * tmp2 * tmp;
    const float fr = f0 + (1 - f0) * tmp5;
    return lerp(base, layer, fr);
}

// Based on GLTF specification:
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation
float3 brdf_f(GLTF_BRDF brdf, in const SurfaceInteraction si, in const float3 wi)
{
    Angles angles;
    angles.NdotL = dot(si.n, wi);
    angles.NdotV = dot(si.n, si.wo);
    const float3 h = normalize(si.wo + wi);
    angles.NdotH = dot(si.n, h);
    angles.LdotH = dot(wi, h);
    angles.VdotH = dot(si.wo, h);

    const float specular = specular_brdf(brdf.alpha, angles);
    const float3 metal_brdf = conductor_fresnel(brdf.baseColor, specular, angles);
    const float3 dielectric_brdf = fresnel_mix(1.5, diffuse_brdf(brdf.baseColor), specular, angles);
    return lerp(dielectric_brdf, metal_brdf, brdf.metallic) * max(angles.NdotL, 0);
}

#endif // __GLTF_BRDF_HLSL__
