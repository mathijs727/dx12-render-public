#include "mesh_shading.hlsl"
#include "ShaderInputs/inputgroups/DefaultLayout/BindlessScene.hlsl"
#include "ShaderInputs/inputgroups/DefaultLayout/MeshShadingBindless.hlsl"

[NumThreads(1, 1, 1)]
void main(uint threadIdx : SV_DispatchThreadID)
{
    const BindlessMeshInstance meshInstance = g_bindlessScene.getMeshInstances()[threadIdx];
    const BindlessMesh mesh = g_bindlessScene.getMeshes()[meshInstance.meshIdx];

    Payload payload;
    payload.mvpMatrix = mul(g_meshShadingBindless.getViewProjectionMatrix(), meshInstance.modelMatrix);
    payload.normalMatrix = meshInstance.normalMatrix;
    payload.meshIdx = meshInstance.meshIdx;
    DispatchMesh(mesh.numMeshlets, 1, 1, payload);
}