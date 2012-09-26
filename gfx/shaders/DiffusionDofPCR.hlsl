#include "DiffusionDof.h"
Texture2D<uint4> g_abcdIn : register(t0);

RWTexture2D<float3> g_yOut : register(u0);

#define TG_SIZE 64
groupshared float gs[6][TG_SIZE]; //abcd - last entry is for dummy 0 values
//awkward parameters b/c we don't want to keep reading the thread's abcd

void setabcd(int idx, float3 abc, float3 d)
{
	gs[0][idx] = abc.x;
	gs[1][idx] = abc.y;
	gs[2][idx] = abc.z;
	gs[3][idx] = d.x;
	gs[4][idx] = d.y;
	gs[5][idx] = d.z;
}
void getabcd(int idx, float multiplier, out float a, out float b, out float c, out float3 d)
{
	int k = idx;
	a = multiplier * gs[0][k];
	b = multiplier * gs[1][k];
	c = multiplier * gs[2][k];
	d = multiplier * float3(gs[3][k], gs[4][k], gs[5][k]);
}
void reduce(int idx0, inout ABCDEntry abcd, int idx2)
{
	ABCDTriple abcd3;
	abcd3.a[1] = abcd.a;
	abcd3.b[1] = abcd.b;
	abcd3.c[1] = abcd.c;
	abcd3.d[1] = abcd.d;
	int outOfBounds0 = (idx0 < 0);
	int outOfBounds1 = (idx2 > TG_SIZE - 1);

	getabcd(idx0, 1 - outOfBounds0, abcd3.a[0], abcd3.b[0], abcd3.c[0], abcd3.d[0]);
	getabcd(idx2, 1 - outOfBounds1, abcd3.a[2], abcd3.b[2], abcd3.c[2], abcd3.d[2]);
	
	GroupMemoryBarrierWithGroupSync();
	abcd = reduce(abcd3);
}
void reduce(int idx0, inout ABCDEntry abcd, int idx2, int writeTo)
{
	reduce(idx0, abcd, idx2);
	setabcd(writeTo, float3(abcd.a, abcd.b, abcd.c), abcd.d);
	GroupMemoryBarrierWithGroupSync();
}
[numthreads(TG_SIZE, 1, 1)]
void ddofPcr(uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	int idx = DTid.x;
	uint4 abcd0 = g_abcdIn[DTid.xy];

	ABCDEntry abcd;
	ddofUnpack(abcd0, abcd.a, abcd.b, abcd.c, abcd.d);
	
	setabcd(idx, float3(abcd.a, abcd.b, abcd.c), abcd.d);
		
	GroupMemoryBarrierWithGroupSync();
	//reduced down to 1 equation
	reduce(idx - 1, abcd, idx + 1, idx);
	reduce(idx - 2, abcd, idx + 2, idx);
	reduce(idx - 4, abcd, idx + 4, idx);
	reduce(idx - 8, abcd, idx + 8, idx);
	reduce(idx - 16, abcd, idx + 16, idx);
	//reduce(idx - 32, abcd, idx + 32, idx);
	//reduce(idx - 64, abcd, idx + 64, idx);
	//reduce(idx - 128, abcd, idx + 128, idx);

	//no need to write the last (only b is non-zero)
	reduce(idx - 32, abcd, idx + 32);
	
	//solve + write out
	//only b is non-zero
	g_yOut[DTid.xy] = abcd.d / abcd.b;
}