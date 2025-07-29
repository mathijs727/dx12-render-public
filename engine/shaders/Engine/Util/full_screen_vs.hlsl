#include "full_screen_vertex.hlsl"

FULL_SCREEN_VS_OUTPUT main(in uint id
               : SV_VertexID)
{
    // https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
    /*
    //See: https://web.archive.org/web/20140719063725/http://www.altdev.co/2011/08/08/interesting-vertex-shader-trick/
 
        1  
    ( 0, 2)
    [-1, 3]   [ 3, 3]
        .
        |`.
        |  `.  
        |    `.
        '------`
        0         2
    ( 0, 0)   ( 2, 0)
    [-1,-1]   [ 3,-1]
 
    ID=0 -> Pos=[-1,-1], Tex=(0,0)
    ID=1 -> Pos=[-1, 3], Tex=(0,2)
    ID=2 -> Pos=[ 3,-1], Tex=(2,0)
    */

    FULL_SCREEN_VS_OUTPUT output;
    output.texCoord.x = (id == 2) ? 2.0 : 0.0;
    output.texCoord.y = (id == 1) ? 2.0 : 0.0;

    output.position = float4(output.texCoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}