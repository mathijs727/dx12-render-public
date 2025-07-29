#include "Engine/Util/printf.hlsl"
#include "TestShaderInputs/inputgroups/TestComputeLayout/TestPrintf.hlsl"

[numthreads(1, 1, 1)] void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    PrintSink printSink = g_testPrintf.getPrintSink();
    printf(printSink, "Hello World = {}", 12);
    g_testPrintf.getOut()[0] = 123;
    //printSink.printBuffer.Store(0, 123);
    
}