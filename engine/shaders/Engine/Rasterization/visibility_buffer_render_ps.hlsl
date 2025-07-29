#include "Engine/Shared/vs_output.hlsl"
#include "Engine/Util/printf.hlsl"
#include "ShaderInputs/inputgroups/DefaultLayout/VisiblityRender.hlsl"
#include "ShaderInputs/inputlayouts/DefaultLayout.hlsl"

cbuffer DrawConstants : ROOT_CONSTANT_DRAWID {
    uint32_t drawID;
};

uint2 main(uint32_t primitiveID : SV_PrimitiveID) : SV_TARGET
{
    //PrintSink printSink = g_visiblityRender.getPrintSink();
    //if(primitiveID == 0)
    //    printf(printSink, "a = {}; b = {}.", 100, 200);
    return uint2(drawID, primitiveID);
}
