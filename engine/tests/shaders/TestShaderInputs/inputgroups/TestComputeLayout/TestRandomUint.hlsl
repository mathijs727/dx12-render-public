#ifndef __TestRandomUint__
#define __TestRandomUint__
cbuffer CONSTANT_DATA : register(b0, space0) {
	uint64_t _seed;
};
RWStructuredBuffer<uint> _out : register(u1, space0);
class TestRandomUint {
	uint64_t getSeed() {
		return _seed;
	}
	RWStructuredBuffer<uint> getOut() {
		return _out;
	}
};
TestRandomUint g_testRandomUint;
#endif
