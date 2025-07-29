#include "ShaderInputs/inputgroups/RayTraceGlobalLayout/PathTracing.hlsl"
#include "ShaderInputs/inputgroups/RayTraceLocalLayout/RTMesh.hlsl"
#include "ShaderInputs/inputgroups/RayTraceLocalLayout/SinglePBRMaterial.hlsl"
#include "ShaderInputs/inputlayouts/RayTraceLocalLayout.hlsl"
#include "ShaderInputs/inputlayouts/RayTraceGlobalLayout.hlsl"
// Inputs first...
#include "Engine/Debug/visual_debug.hlsl"
#include "Engine/Materials/gltf_brdf.hlsl"
#include "Engine/Lights/directional_light.hlsl"
#include "rt_camera.hlsl"
#include "Engine/Util/random.hlsl"
#include "Engine/Shared/constants.hlsl"

#define MAX_NUM_BOUNCES 4
#define epsilon 0.00001

// Ray payload.
struct HitInfo {
    float3 color;
    int recursionDepth;
    RNG rng;
};
struct ShadowPayload {
    bool visible;
};

#if DRAW_DEBUG_RAYS
void debugDrawArrow(const in float3 from, const in float3 to, const in float3 color) {
    const int2 launchIndex = DispatchRaysIndex().xy;
    if (all(launchIndex == g_pathTracing.getDebugPixel())) {
        visualDebugDrawArrow(g_pathTracing.getVisualDebug(), from, to, color);
    }
}
void debugDrawRay(in const RayDesc ray, float3 color) {
    const int2 launchIndex = DispatchRaysIndex().xy;
    if (all(launchIndex == g_pathTracing.getDebugPixel())) {
        visualDebugDrawArrow(g_pathTracing.getVisualDebug(), ray.Origin, ray.Origin + ray.TMax * ray.Direction, color);
    }
}
void debugDrawCurrentRay(float3 color) {
    const int2 launchIndex = DispatchRaysIndex().xy;
    if (all(launchIndex == g_pathTracing.getDebugPixel())) {
        visualDebugDrawArrow(g_pathTracing.getVisualDebug(), WorldRayOrigin(), WorldRayOrigin() + RayTCurrent() * WorldRayDirection(), color);
    }
}
#endif

[shader("raygeneration")] void rayGen() {
    const PathTracing inputs = g_pathTracing;

    const uint2 launchIndex = DispatchRaysIndex().xy;
    const float2 dims = float2(DispatchRaysDimensions().xy);
    // Remap launch index to the range of [-1, +1)
    const float2 d = ((launchIndex.xy + 0.5f) / dims.xy) * 2.0f - 1.0f;

    // Initialize the ray payload.
    HitInfo payload;
    payload.recursionDepth = 1;
    payload.rng = createRandomNumberGenerator(inputs.getRandomSeed(), launchIndex.y * dims.x + launchIndex.x);

    RayDesc ray;
    generateRay(inputs.getCamera(), d, ray);
    ray.TMin = 0;
    ray.TMax = 100000;
    TraceRay(
        inputs.getAccelerationStructure(),
        RAY_FLAG_NONE,
        0xFF, // InstanceInclusionMask
        0, // RayContributionToHitGroupIndex (ray id)
        1, // MultiplierForGeometryContributionToHitGroupIndex
        0, // Miss shader index
        ray,
        payload);

    if (isnan(payload.color.x) || isnan(payload.color.y) || isnan(payload.color.z)) {
        // If the color is NaN, set it to black.
        payload.color = float3(0, 0, 0);
    }

    RWTexture2D<float4> output = inputs.getOutput();
    // Write the color to the output texture.
    if (inputs.getOverwriteOutput()) {
        output[launchIndex] = float4(payload.color, 1.0f);
    } else {
        output[launchIndex] += float4(payload.color, 0.0f);
    }
}

[shader("closesthit")] void pbrClosestHit(inout HitInfo payload, BuiltInTriangleIntersectionAttributes attrib) {
    const StructuredBuffer<uint32_t> indices = g_rTMesh.getIndices();
    const StructuredBuffer<Vertex> vertices = g_rTMesh.getVertices();

#if DRAW_DEBUG_RAYS
    if (payload.recursionDepth > 1)
        debugDrawCurrentRay(GREEN);
#endif

    const uint32_t baseIndex = 3 * PrimitiveIndex();
    const Vertex v0 = vertices[indices[baseIndex + 0]];
    const Vertex v1 = vertices[indices[baseIndex + 1]];
    const Vertex v2 = vertices[indices[baseIndex + 2]];

    const float3 baryWeights = float3(1.0f - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y);
    
    // Interpolate texture coordinate & world space position.
    const float2 uv = baryWeights.x * v0.texCoord + baryWeights.y * v1.texCoord + baryWeights.z * v2.texCoord;
    const float3 pObject = baryWeights.x * v0.pos + baryWeights.y * v1.pos + baryWeights.z * v2.pos;
    const float3 pWorld = mul(ObjectToWorld3x4(), float4(pObject, 1));

    // Interpolate [shading] normals.
    const float3x3 normalObjectToWorld = transpose((float3x3)WorldToObject3x4());
    const float3 e1Object = v1.pos - v0.pos;
    const float3 e2Object = v2.pos - v0.pos;
    const float3 gnObject = cross(e1Object, e2Object);
    const float3 gnWorld = normalize(mul(normalObjectToWorld, gnObject));
    const float3 gtObject = cross(gnObject, e1Object);
    const float3 gtWorld = normalize(mul(normalObjectToWorld, gtObject));
    const float3 snObject = baryWeights.x * v0.normal + baryWeights.y * v1.normal + baryWeights.z * v2.normal;
    const float3 snWorld = normalize(mul(normalObjectToWorld, snObject));

    SurfaceInteraction si;
    si.p = pWorld;
    si.n = snWorld;
    si.gn = gnWorld;
    si.gt = gtWorld;
    si.uv = uv;
    si.wo = -WorldRayDirection();

    GLTF_BRDF brdf;
    if (!brdf_construct(si, g_singlePBRMaterial, brdf)) {
        payload.color = float3(1, 0, 0);
        return;
    }

    float3 Lo = float3(0.0f, 0.0f, 0.0f);
    // Next Event Estimation (NEE)
    //for (uint i = 0; i < g_pathTracing.getNumDirectionalLights(); i++) {
    {
        const DirectionalLight sun = g_pathTracing.getSun();
        RayDesc shadowRay;
        const LightSample lightSample = light_sample(sun, si, shadowRay);
        //if (isDummyLightSample(lightSample))
        //    continue;
        
        ShadowPayload shadowPayload;
        shadowPayload.visible = false;
        if (dot(si.gn, lightSample.wi) > 0.0) {
            TraceRay(
                g_pathTracing.getAccelerationStructure(),
                RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                0xFF, // InstanceInclusionMask
                0, // RayContributionToHitGroupIndex (ray id)
                1, // MultiplierForGeometryContributionToHitGroupIndex
                1, // Miss shader index
                shadowRay,
                shadowPayload);
        }

#if DRAW_DEBUG_RAYS
        debugDrawRay(shadowRay, shadowPayload.visible ? WHITE : BLACK);
#endif
        if (shadowPayload.visible) {
            const float3 f = brdf_f(brdf, si, lightSample.wi);
            Lo += lightSample.luminance * f / lightSample.pdf;
        }
    }

    if (payload.recursionDepth < MAX_NUM_BOUNCES) { // -1 to account for NEE shadow rays.
        const float2 uv = payload.rng.generateFloat2();
        float pdf;

        HitInfo continuationPayload;
        continuationPayload.recursionDepth = payload.recursionDepth + 1;
        continuationPayload.rng = payload.rng;

        float3 wiObject;
        brdf_sample(uv, wiObject, pdf);
        const float3 wiWorld = si.surfaceToWorld(wiObject);
        const float3 f = brdf_f(brdf, si, wiWorld);

        RayDesc continuationRay;
        continuationRay.Origin = si.p + epsilon * si.n;
        continuationRay.Direction = wiWorld;
        continuationRay.TMin = 0;
        continuationRay.TMax = 100000;
        TraceRay(
            g_pathTracing.getAccelerationStructure(),
            RAY_FLAG_NONE,
            0xFF, // InstanceInclusionMask
            0, // RayContributionToHitGroupIndex (ray id)
            1, // MultiplierForGeometryContributionToHitGroupIndex
            0, // Miss shader index
            continuationRay,
            continuationPayload);
        Lo += f * continuationPayload.color / pdf;
    }

    payload.color = Lo;
}

[shader("anyhit")] void pbrAnyHit(inout HitInfo payload, BuiltInTriangleIntersectionAttributes attrib) {
    const StructuredBuffer<uint32_t> indices = g_rTMesh.getIndices();
    const StructuredBuffer<Vertex> vertices = g_rTMesh.getVertices();
    const Texture2D baseColorTexture = g_singlePBRMaterial.getBaseColorTexture();

    const uint32_t baseIndex = 3 * PrimitiveIndex();
    const Vertex v0 = vertices[indices[baseIndex + 0]];
    const Vertex v1 = vertices[indices[baseIndex + 1]];
    const Vertex v2 = vertices[indices[baseIndex + 2]];

    const float2 texCoord = v0.texCoord * (1.0f - attrib.barycentrics.x - attrib.barycentrics.y) + attrib.barycentrics.x * v1.texCoord + attrib.barycentrics.y * v2.texCoord;
    const float alpha = baseColorTexture.SampleLevel(g_materialSampler, texCoord, 0).w;
    if (alpha < 1.0)
        IgnoreHit();
}

// https://pbr-book.org/3ed-2018/Color_and_Radiometry/Working_with_Radiometric_Integrals#SphericalPhi
float SphericalTheta(in const float3 v) {
    return acos(clamp(v.y, -1, 1));
}
float SphericalPhi(in const float3 v) {
    float p = atan2(v.z, v.x);
    return (p < 0) ? (p + 2 * PI) : p;
}

[shader("miss")]
void miss(inout HitInfo payload) {
#if DRAW_DEBUG_RAYS
    if (payload.recursionDepth > 0)
        debugDrawCurrentRay(RED);
#endif

    const float environmentMapStrength = g_pathTracing.getEnvironmentMapStrength();
    if (environmentMapStrength == 0.0f) {
        payload.color = float3(0, 0, 0);
        return;
    }

    const float s = SphericalPhi(-WorldRayDirection()) * ONE_OVER_TWO_PI; 
    const float t = SphericalTheta(WorldRayDirection()) * ONE_OVER_PI; 
    payload.color = g_pathTracing.getEnvironmentMap().SampleLevel(g_environmentMapSampler, float2(s, t), 0).rgb * environmentMapStrength;
}

[shader("miss")]
void shadowMiss(inout ShadowPayload payload) {
    payload.visible = true;
}
