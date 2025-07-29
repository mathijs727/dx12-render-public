#include "ShaderInputs/structs/RTScreenCamera.hlsl"

void generateRayNoNormalize(in RTScreenCamera camera, in float2 uv, inout RayDesc ray) {
	ray.Origin = camera.origin;
	ray.Direction =  camera.forward + uv.x * camera.screenU + -uv.y * camera.screenV;
}

void generateRay(in RTScreenCamera camera, in float2 uv, inout RayDesc ray) {
	generateRayNoNormalize(camera, uv, ray);
	ray.Direction = normalize(ray.Direction);
}

