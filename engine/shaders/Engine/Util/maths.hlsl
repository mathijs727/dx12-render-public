float4x4 make_orthonal_basis(float3 up) {
    const float3 tangent = normalize(dot(up, float3(1, 0, 0)) < 0.5f ? cross(up, float3(1, 0, 0)) : cross(up, float3(0, 1, 0)));
    const float3 bitangent = cross(up, tangent);
    return transpose(float4x4(
        float4(tangent, 0),
        float4(up, 0),
        float4(bitangent, 0),
        float4(0, 0, 0, 1)
    ));
}

float4x4 make_translation_float4x4(float3 t) {
    return float4x4(
        1, 0, 0, t.x,
        0, 1, 0, t.y,
        0, 0, 1, t.z,
        0, 0, 0, 1
    );
}

#define ALIGN32(n) (((n) + 31) & ~31)

