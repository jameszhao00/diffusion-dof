#include "DiffusionDof.h"
Texture2D<uint4> g_abcdIn : register(t0);

RWTexture2D<float3> g_yOut : register(u0);

#define TG_SIZE 64
groupshared float gs[6][TG_SIZE];
//awkward parameters b/c we don't want to keep reading the thread's abcd

void reduce(int idx0, inout ABCDEntry abcd, int idx2)
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
		abcd3.a[0] = gs[0][idx0];
		abcd3.b[0] = gs[1][idx0];
		abcd3.c[0] = gs[2][idx0];
		abcd3.d[0] = float3(gs[3][idx0], gs[4][idx0], gs[5][idx0]);
	}
	if(idx2 > TG_SIZE - 1)
	{
		abcd3.a[2] = 0; abcd3.b[2] = 0; abcd3.c[2] = 0; abcd3.d[2] = 0;		
	}
	else
	{
		abcd3.a[2] = gs[0][idx2];
		abcd3.b[2] = gs[1][idx2];
		abcd3.c[2] = gs[2][idx2];
		abcd3.d[2] = float3(gs[3][idx2], gs[4][idx2], gs[5][idx2]);
	}
	abcd = reduce(abcd3);
}
void reduce(int idx0, inout ABCDEntry abcd, int idx2, int writeTo)
{
	reduce(idx0, abcd, idx2);
	gs[0][writeTo] = abcd.a;
	gs[1][writeTo] = abcd.b;
	gs[2][writeTo] = abcd.c;
	gs[3][writeTo] = abcd.d.x;
	gs[4][writeTo] = abcd.d.y;
	gs[5][writeTo] = abcd.d.z;
}
[numthreads(TG_SIZE, 1, 1)]
void ddofPcr(uint3 DTid : SV_DispatchThreadID)
{
	int idx = DTid.x;
	uint4 abcd0 = g_abcdIn[DTid.xy];

	ABCDEntry abcd;
	ddofUnpack(abcd0, abcd.a, abcd.b, abcd.c, abcd.d);
	gs[0][idx] = abcd.a;
	gs[1][idx] = abcd.b;
	gs[2][idx] = abcd.c;
	gs[3][idx] = abcd.d.x;
	gs[4][idx] = abcd.d.y;
	gs[5][idx] = abcd.d.z;


	GroupMemoryBarrierWithGroupSync();
	//reduced down to 1 equation
	reduce(idx - 1, abcd, idx + 1, idx);
	GroupMemoryBarrierWithGroupSync();
	reduce(idx - 2, abcd, idx + 2, idx);
	GroupMemoryBarrierWithGroupSync();
	reduce(idx - 4, abcd, idx + 4, idx);
	GroupMemoryBarrierWithGroupSync();
	reduce(idx - 8, abcd, idx + 8, idx);
	GroupMemoryBarrierWithGroupSync();
	reduce(idx - 16, abcd, idx + 16, idx);
	GroupMemoryBarrierWithGroupSync();
	//reduce(idx - 32, abcd, idx + 32, idx);
	//GroupMemoryBarrierWithGroupSync();
	//reduce(idx - 64, abcd, idx + 64, idx);
	//GroupMemoryBarrierWithGroupSync();
	//reduce(idx - 128, abcd, idx + 128, idx);
	//GroupMemoryBarrierWithGroupSync();

	//no need to write the last (only b is non-zero)
	reduce(idx - 32, abcd, idx + 32);
	
	//solve + write out
	//only b is non-zero
	g_yOut[DTid.xy] = abcd.d / abcd.b;
}