struct Payload {
	float4x4 mvpMatrix;
	float3x3 normalMatrix;
	uint32_t meshIdx;
	uint32_t meshletStart;
};