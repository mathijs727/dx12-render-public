#ifndef __TestPrintf__
#define __TestPrintf__
#include "..\\..\\..\\..\\..\\shaders\\ShaderInputs\\groups\\PrintSink.hlsl"
cbuffer CONSTANT_DATA : register(b0, space0) {
	uint32_t ___printSink_bufferSize;
	uint32_t ___printSink_paused;
};
RWStructuredBuffer<uint> _out : register(u1, space0);
RWByteAddressBuffer ___printSink_printBuffer : register(u2, space0);
class TestPrintf {
	PrintSink getPrintSink() {
		PrintSink outGroup;
		outGroup.printBuffer = get__printSink_printBuffer();
		outGroup.bufferSize = get__printSink_bufferSize();
		outGroup.paused = get__printSink_paused();
		return outGroup;
;
	}
	RWStructuredBuffer<uint> getOut() {
		return _out;
	}
	RWByteAddressBuffer get__printSink_printBuffer() {
		return ___printSink_printBuffer;
	}
	uint32_t get__printSink_bufferSize() {
		return ___printSink_bufferSize;
	}
	uint32_t get__printSink_paused() {
		return ___printSink_paused;
	}
};
TestPrintf g_testPrintf;
#endif
