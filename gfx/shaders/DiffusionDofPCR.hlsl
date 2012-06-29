#include "DiffusionDof.h"
Texture2D<uint4> g_abcdIn : register(t0);

RWTexture2D<float3> g_yOut : register(u0);

#define TG_SIZE 64
groupshared uint4 gs[TG_SIZE];
//awkward parameters b/c we don't want to keep reading the thread's abcd
void reduce(int idx0, inout ABCDEntry abcd, int idx2, inout uint4 writeTo)
{
	ABCDTriple abcd3;
	abcd3.a[1] = abcd.a;
	abcd3.b[1] = abcd.b;
	abcd3.c[1] = abcd.c;
	abcd3.d[1] = abcd.d;
	//TODO: out of bounds checks (if CS5.0 is undefined)
	if(idx0 < 0)
	{
		abcd3.a[0] = 0; abcd3.b[0] = 0; abcd3.c[0] = 0; abcd3.d[0] = 0;
	}
	else
	{
		ddofUnpack(gs[idx0], abcd3.a[0], abcd3.b[0], abcd3.c[0], abcd3.d[0]);
	}
	if(idx2 > TG_SIZE - 1)
	{
		abcd3.a[2] = 0; abcd3.b[2] = 0; abcd3.c[2] = 0; abcd3.d[2] = 0;		
	}
	else
	{
		ddofUnpack(gs[idx2], abcd3.a[2], abcd3.b[2], abcd3.c[2], abcd3.d[2]);
	}
	abcd = reduce(abcd3);
	writeTo = ddofPack(abcd.a, abcd.b, abcd.c, abcd.d);
}
[numthreads(TG_SIZE, 1, 1)]
void ddofPcr(uint3 DTid : SV_DispatchThreadID)
{
	int idx = DTid.x;
	uint4 abcd0 = g_abcdIn[DTid.xy];
	gs[idx] = abcd0;
	GroupMemoryBarrierWithGroupSync();
	ABCDEntry abcd;
	ddofUnpack(abcd0, abcd.a, abcd.b, abcd.c, abcd.d);
	//reduced down to 1 equation
	reduce(idx - 1, abcd, idx + 1, gs[idx]);
	GroupMemoryBarrierWithGroupSync();
	reduce(idx - 2, abcd, idx + 2, gs[idx]);
	GroupMemoryBarrierWithGroupSync();
	reduce(idx - 4, abcd, idx + 4, gs[idx]);
	GroupMemoryBarrierWithGroupSync();
	reduce(idx - 8, abcd, idx + 8, gs[idx]);
	GroupMemoryBarrierWithGroupSync();
	reduce(idx - 16, abcd, idx + 16, gs[idx]);
	GroupMemoryBarrierWithGroupSync();

	//no need to write the last (only b is non-zero)
	uint4 dummy;
	reduce(idx - 32, abcd, idx + 32, dummy);
	
	//solve + write out
	//only b is non-zero
	g_yOut[DTid.xy] = abcd.d / abcd.b;
}