#ifndef __SURFACE_INTERACTION_HLSL__
#define __SURFACE_INTERACTION_HLSL__

struct SurfaceInteraction {
    float3 p; // position
    float3 n; // shading normal
    float3 gn; // geometric normal
    float3 gt; // geometric tangent
    float2 uv; // texture coordinates
    float3 wo; // view direction

    float3 surfaceToWorld(in const float3 direction) {
        const float3 bitangent = cross(gn, gt);
        return direction.x * gt + direction.y * bitangent + direction.z * gn;
    }
};

#endif // __SURFACE_INTERACTION_HLSL__
