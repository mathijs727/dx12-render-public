#include "ShaderInputs/inputgroups/DefaultLayout/BindlessScene.hlsl"
#include "ShaderInputs/inputgroups/DefaultLayout/VisiblityToGBuffer.hlsl"
#include "ShaderInputs/inputlayouts/DefaultLayout.hlsl"
#include "Engine/RayTracing/rt_camera.hlsl"
#include "Engine/Util/full_screen_vertex.hlsl"
#include "Engine/Shared/surface_interaction.hlsl"
#include "Engine/Materials/gltf_brdf.hlsl"
#include "Engine/Util/random.hlsl"

void transformVertex(in const BindlessMeshInstance instance, inout Vertex vertex) {
    vertex.pos = mul(instance.modelMatrix, float4(vertex.pos, 1)).xyz;
    vertex.normal = mul(instance.normalMatrix, vertex.normal);
}

struct PS_OUTPUT
{
    float4 position_metallic: SV_Target0;
    float4 normal_alpha : SV_Target1;
    float4 baseColor: SV_Target2;
};
PS_OUTPUT main(FULL_SCREEN_VS_OUTPUT vertex)
{
    const int3 pixel = int3(vertex.position.xy, 0);
    const float2 pixelUV = (vertex.position.xy + float2(0.0f, 0.0f)) * g_visiblityToGBuffer.getInvResolution() * 2.0f - 1.0f;
    PrintSink printSink = g_visiblityToGBuffer.getPrintSink();

    const uint2 drawAndPrimitiveID = g_visiblityToGBuffer.getVisibilityBuffer().Load(pixel);
    const uint32_t instanceID = drawAndPrimitiveID.x >> 16;
    const uint32_t subMeshID = drawAndPrimitiveID.x & 0xFFFF;
    const uint32_t primitiveID = drawAndPrimitiveID.y;
    
    const BindlessMeshInstance meshInstance = g_bindlessScene.getMeshInstances()[instanceID];
    const StructuredBuffer<uint32_t> indexBuffer = g_bindlessScene.getIndexBuffers(meshInstance.meshIdx);
    const StructuredBuffer<Vertex> vertexBuffer = g_bindlessScene.getVertexBuffers(meshInstance.meshIdx);
    const BindlessMesh mesh = g_bindlessScene.getMeshes()[meshInstance.meshIdx];
    const BindlessSubMesh subMesh = g_bindlessScene.getSubMeshes()[mesh.subMeshStart + subMeshID];
    const uint32_t indexStart = subMesh.indexStart + primitiveID * 3;

    const uint32_t i0 = indexBuffer[indexStart + 0] + subMesh.baseVertex;
    const uint32_t i1 = indexBuffer[indexStart + 1] + subMesh.baseVertex;
    const uint32_t i2 = indexBuffer[indexStart + 2] + subMesh.baseVertex;
    Vertex v0 = vertexBuffer[i0];
    Vertex v1 = vertexBuffer[i1];
    Vertex v2 = vertexBuffer[i2];
    transformVertex(meshInstance, v0);
    transformVertex(meshInstance, v1);
    transformVertex(meshInstance, v2);

    const RTScreenCamera camera = g_visiblityToGBuffer.getCamera();
    RayDesc cameraRay;
    generateRayNoNormalize(camera, pixelUV, cameraRay);

    float lambda1, lambda2;
    float2 ddx, ddy;
    if (g_visiblityToGBuffer.getRayDifferentialsChristoph()) {
        // Based on:
        // https://github.com/MomentsInGraphics/vulkan_renderer/blob/main/src/shaders/shading_pass.frag.glsl
        //
        // Perform ray triangle intersection to figure out barycentrics within the triangle
	    float3 barycentrics;
	    float3 edges[2] = {
		    v1.pos - v0.pos,
		    v2.pos - v0.pos
	    };
	    float3 ray_cross_edge_1 = cross(cameraRay.Direction, edges[1]);
	    float rcp_det_edges_direction = 1.0f / dot(edges[0], ray_cross_edge_1);
	    float3 ray_to_0 = cameraRay.Origin - v0.pos;
	    float det_0_dir_edge_1 = dot(ray_to_0, ray_cross_edge_1);
	    barycentrics.y = rcp_det_edges_direction * det_0_dir_edge_1;
	    float3 edge_0_cross_0 = cross(edges[0], ray_to_0);
	    float det_dir_edge_0_0 = dot(cameraRay.Direction, edge_0_cross_0);
	    barycentrics.z = -rcp_det_edges_direction * det_dir_edge_0_0;
	    barycentrics.x = 1.0f - (barycentrics.y + barycentrics.z);
	    // Compute screen space derivatives for the barycentrics
	    float3 barycentrics_derivs[2];
	    [[unroll]]
	    for (uint i = 0; i != 2; ++i) {
		    float3 ray_direction_deriv = g_visiblityToGBuffer.getPixelToRayDirectionWorldSpace(i);
		    float3 ray_cross_edge_1_deriv = cross(ray_direction_deriv, edges[1]);
		    float rcp_det_edges_direction_deriv = -dot(edges[0], ray_cross_edge_1_deriv) * rcp_det_edges_direction * rcp_det_edges_direction;
		    float det_0_dir_edge_1_deriv = dot(ray_to_0, ray_cross_edge_1_deriv);
		    barycentrics_derivs[i].y = rcp_det_edges_direction_deriv * det_0_dir_edge_1 + rcp_det_edges_direction * det_0_dir_edge_1_deriv;
		    float det_dir_edge_0_0_deriv = dot(ray_direction_deriv, edge_0_cross_0);
		    barycentrics_derivs[i].z = -rcp_det_edges_direction_deriv * det_dir_edge_0_0 - rcp_det_edges_direction * det_dir_edge_0_0_deriv;
		    barycentrics_derivs[i].x = -(barycentrics_derivs[i].y + barycentrics_derivs[i].z);
	    }
	    // Interpolate vertex attributes across the triangle
        lambda1 = barycentrics.y;
        lambda2 = barycentrics.z;

	    // Compute screen space texture coordinate derivatives for filtering
        float2 tex_coords[3] = {
            v0.texCoord, v1.texCoord, v2.texCoord
        };
	    float2 tex_coord_derivs[2] = { float2(0.0f, 0.0f), float2(0.0f, 0.0f) };
	    [[unroll]]
	    for (uint i = 0; i != 2; ++i)
		    [[unroll]]
		    for (uint j = 0; j != 3; ++j)
			    tex_coord_derivs[i] += barycentrics_derivs[i][j] * tex_coords[j];
        ddx = tex_coord_derivs[0];
        ddy = tex_coord_derivs[1];
    } else {
        // Consider how the hitpoint between the ray and triangle can be described with barycentric coordinates lambda_i:
        // P = v0 + lambda1*edge1 + lamdba2*edge2
        //    (where edge1 = (v1 - v0) & edge2 = (v2 - v0) )
        //
        // The ray is described as a function o + t*d, where o is the origin and d the direction.
        // Plugging this into our triangle interpolation formula, we get:
        // o + t*d = v0 + lambda1*edge1 + lambda2*edge2
        //
        // Moving all unknowns to the left of the equal sign, and known values to the right:
        // -t*d + lambda1*edge1 + lambda2*edge2 = o - v0
        //
        // This creates a 3x3 linear system of equations Ax=b:
        // [ |  |  | ] (   t   ) =  |
        // [ -d e1 e2] (lambda1) = o-v0
        // [ |  |  | ] (lambda2) =  |
        //
        // This matrix can be solved by Cramer rule:
        // t = determinant(A) / determinant(A with first column replaced by o-v0)
        // lambda1 = determinant(A) / determinant(A with second column replaced by o-v0)
        // lambda2 = determinant(A) / determinant(A with third column replaced by o-v0)
        //
        // To compute the 3D determinant we can use the cross & dot products
        // determinant(A) = dot(cross(d, e1), e2)
        //
        // The cross product gives a vector orthogonal to d & e1, with its length the area of the parallelogram spanned by d & e1.
        // The dot product then projects e2 on this orthogonal vector, and multiplies it by the length of the vector (area of parallelogram).
        // This effectively computes the area of the parallelogram *times* the height of e2 from the parallelogram,
        //  which is equal to the volume of the parallelepiped spanned by (d, e1, e2).
        const float3 edge1 = v1.pos - v0.pos;
        const float3 edge2 = v2.pos - v0.pos;
        const float3 rayCrossEdge1 = cross(-cameraRay.Direction, edge1);
        const float3 rayCrossEdge2 = cross(-cameraRay.Direction, edge2);
        const float detEdgesDirection = dot(rayCrossEdge1, edge2);
        const float rcpDetEdgesDirection = 1.0f / detEdgesDirection;
        const float3 originMinusV0 = cameraRay.Origin - v0.pos;
        // We need to compute determinant over [-d, x, e2], but we are now computing determinant over [-d, e2, x].
        // Because we reordered the columns we have to negate lambda 1.
        const float detRayEdge2O = dot(rayCrossEdge2, originMinusV0);
        const float detRayEdge1O = dot(rayCrossEdge1, originMinusV0);
        lambda1 = -detRayEdge2O * rcpDetEdgesDirection;
        lambda2 = detRayEdge1O * rcpDetEdgesDirection;

        // Derivation for the derivative of lambda 1:
        // f(x) = -1 * ((-d X e2).(o-v0)) / ((-d X e1).e2)
        //      = -1 * a/b
        // f'(x) = -1 * (a'*b - a*b')/b^2                        QUOTIENT RULE
        //
        // a = (-d X e2) . (o-v0)
        // a' = (-d X e2) . (o-v0)' + (-d X e2)' . (o-v0)        PRODUCT RULE
        //                    0
        // a' = (-d X e2)' . (o-v0)
        // a' = (-d X e2' + -d' x e2) . (o-v0)                   PRODUCT RULE
        //             0
        // a' = (-d' X e2) . (o-v0)
        // a' = (-I X e2) . (o-v0)                               derivative of vector w.r.t vector gives a Jacobian, in this case the (negative) identity matrix.
        // a' = -I . (e2 X (o-v0))                               (a x b) . c == a . (b x c)
        // a' = -I^T (e2 X (o-v0))                               dot product is just a matrix multiplication: a . b = a^T b
        //
        // b = (-d X e1) . e2
        // b' = (-d X e1) . e2' + (-d X e1)' . e2                PRODUCT RULE
        //                   0
        // b' = (-d X e1)' . e2
        // b' = (-d X e1' + -d' X e1) . e2                       PRODUCT RULE
        //             0
        // b' = (-d' X e1) . e2
        // b' = (-I X e1) . e2
        // b' = -I . (e1 X e2)
        // b' = -I^T (e1 X e2)
        //
        // => f'(x) = -1 * (a'*b - a*b')/b^2
        //
        // This computes the derivative of lambda1 with respect to the ray direction dL1dD.
        // For ray differentials we need the derivative of texCoord with respect to the ray direction dSdD.
        // Luckily, the derivative of the texCoord with respect to lambda is trivial:
        //  (texCoord1 - texCoord0) for lambda1 and (texCoord2 - texCoord0) for lambda 2.
        float3 dL0dD, dL1dD, dL2dD;
        const float3 dBdD = -cross(edge1, edge2);
        {
            //const float3 dAdD = -cross(edge2, originMinusV0);
            //dL1dD = -1 * (dAdD*detEdgesDirection - detRayEdge2O*dBdD) * rcpDetEdgesDirection * rcpDetEdgesDirection;
            const float3 dAdD = cross(edge2, originMinusV0);
            dL1dD = (detRayEdge2O*dBdD + dAdD*detEdgesDirection) * rcpDetEdgesDirection * rcpDetEdgesDirection;
        }
        {
            const float3 dAdD = -cross(edge1, originMinusV0);
            dL2dD = (dAdD*detEdgesDirection - detRayEdge1O*dBdD) * rcpDetEdgesDirection * rcpDetEdgesDirection;
        }
        dL0dD = -dL1dD - dL2dD;

        // Derivatives of texture coordinates w.r.t ray direction.
        // S = L0 * S0 + L1 * S1 + L2 * S2, where S_i is the first texture coordinate of vertex i, and L_i are the barycentric coordinates (lambda).
        // dS/dD = S0*dL0/dD + S1*dL1/dD + S2*dL2/dD
        const float3 dSdD = dL0dD * v0.texCoord.x + dL1dD * v1.texCoord.x + dL2dD * v2.texCoord.x;
        const float3 dTdD = dL0dD * v0.texCoord.y + dL1dD * v1.texCoord.y + dL2dD * v2.texCoord.y;
    
        // Derivatives of texture coordinates w.r.t. pixel coordinates.
        // dS/dx = dS/dD * dD/dx, dS/dy = dS/dD * dD/dy
        // dT/dx = dT/dD * dD/dx, dT/dy = dT/dD * dD/dy
        const float3 dDdX = g_visiblityToGBuffer.getPixelToRayDirectionWorldSpace(0);
        const float3 dDdY = g_visiblityToGBuffer.getPixelToRayDirectionWorldSpace(1);

        ddx = float2(dot(dSdD, dDdX), dot(dTdD, dDdX));
        ddy = float2(dot(dSdD, dDdY), dot(dTdD, dDdY));
    }

    const float lambda0 = 1.0f - lambda1 - lambda2;
    const float2 texCoord = lambda0 * v0.texCoord + lambda1 * v1.texCoord + lambda2 * v2.texCoord;
    const float3 shadingNormal = normalize(lambda0 * v0.normal + lambda1 * v1.normal + lambda2 * v2.normal);
    const float3 p = lambda0 * v0.pos + lambda1 * v1.pos + lambda2 * v2.pos;

    const Texture2D<float4> baseColorTexture = g_bindlessScene.getBaseColorTextures(subMesh.material.baseColorTextureIdx);
    float3 baseColor;
    if (g_visiblityToGBuffer.getOutputType() == 0) {
        baseColor = subMesh.material.baseColor * baseColorTexture.SampleLevel(g_materialSampler, texCoord, 0).xyz;
    } else if (g_visiblityToGBuffer.getOutputType() == 1) {
        baseColor = subMesh.material.baseColor * baseColorTexture.SampleGrad(g_materialSampler, texCoord, ddx, ddy).xyz;
    } else if (g_visiblityToGBuffer.getOutputType() == 2) {
        baseColor = float3(abs(ddx) * 10, 0);
    } else {
        baseColor = float3(abs(ddy) * 10, 0);
    }

    PS_OUTPUT output;
    output.position_metallic = float4(p, subMesh.material.metallic);
    output.normal_alpha = float4(shadingNormal, subMesh.material.alpha);
    output.baseColor = float4(baseColor, 1);
    return output;
}