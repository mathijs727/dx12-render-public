#ifndef __TestRandomFloat__
#define __TestRandomFloat__
cbuffer CONSTANT_DATA : register(b0, space0) {
	uint64_t _seed;
};
RWStructuredBuffer<float> _out : register(u1, space0);
class TestRandomFloat {
	uint64_t getSeed() {
		return _seed;
	}
	RWStructuredBuffer<float> getOut() {
		return _out;
	}
};
TestRandomFloat g_testRandomFloat;
#endif
