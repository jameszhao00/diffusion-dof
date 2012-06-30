#include "DiffusionDof.h"
Texture2D<float> g_depth : register(t0);
Texture2D<float3> g_color : register(t1);

Texture2D<uint4> g_abcdIn : register(t2);
RWTexture2D<uint4> g_abcdOut : register(u0);
void updateABCD(int2 xy, ABCDTriple abcd)
{
	//b will never be 0 unless it doesn't exist!

	//m[] needs to be aware of boundaries
	float m0 = abcd.b[0] == 0 ? 0 : -(abcd.a[1] / abcd.b[0]);
	float m1 = abcd.b[2] == 0 ? 0 : -(abcd.c[1] / abcd.b[2]);

	float a = m0 * abcd.a[0];
	float b = abcd.b[1] + m0 * abcd.c[0] + m1 * abcd.a[2];
	float c = m1 * abcd.c[2];
	//todo: take care of endpoints!
	//i think it does at the moment, but confirm
	float3 d = abcd.d[1] + m0 * abcd.d[0] + m1 * abcd.d[2];
	g_abcdOut[xy] = ddofPack(a, b, c, d);
}
#define PASSN_NUMTHREADS 64
groupshared uint4 passn_gsmem[PASSN_NUMTHREADS * 2]; //for depth,color
[numthreads(PASSN_NUMTHREADS, 1, 1)]
void csPass1H(uint3 Gid : SV_GroupId, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	//every threadgroup has a buffer at the end

	int passIdx = g_ddofVals.z;
	float2 size = g_ddofVals2.xy;	
	//ABCDTriple abcd = readABCD(g_abcdIn, xy, int2(1, 0));		
	int i0 = Gid.x * (PASSN_NUMTHREADS - 1) * 2;
	[unroll]
	for(int i = 0; i < 2; i++)
	{
		int readIdx = GTid.x + i0 + PASSN_NUMTHREADS * i;
		int writeIdx = GTid.x + PASSN_NUMTHREADS * i;
		uint4 abcd = g_abcdIn[int2(readIdx, DTid.y)];
		passn_gsmem[writeIdx] = abcd;
	}
	GroupMemoryBarrierWithGroupSync();
	if(GTid.x == PASSN_NUMTHREADS - 1) return;

	ABCDTriple abcd3;
	ddofUnpack(passn_gsmem[GTid.x * 2], abcd3.a[0], abcd3.b[0],  abcd3.c[0],  abcd3.d[0]);
	ddofUnpack(passn_gsmem[GTid.x * 2 + 1], abcd3.a[1], abcd3.b[1],  abcd3.c[1],  abcd3.d[1]);
	ddofUnpack(passn_gsmem[GTid.x * 2 + 2], abcd3.a[2], abcd3.b[2],  abcd3.c[2],  abcd3.d[2]);
	updateABCD(int2(Gid.x * (PASSN_NUMTHREADS - 1) + GTid.x, DTid.y), abcd3);
}

#define PASS0_NUMTHREADS 64

//weird [2] thing is to mitigate bank conflicts
groupshared float p0gs_c[3][PASS0_NUMTHREADS * 4]; //color
groupshared float p0gs_b[PASS0_NUMTHREADS * 4]; //beta

void setc(int idx, float3 c)
{
	p0gs_c[0][idx] = c.x; p0gs_c[1][idx] = c.y; p0gs_c[2][idx] = c.z; 
}
float3 getc(int idx)
{
	return float3(p0gs_c[0][idx], p0gs_c[1][idx], p0gs_c[2][idx]);
}

[numthreads(PASS0_NUMTHREADS, 1, 1)]
void csPass1HPass0(uint3 Gid : SV_GroupId, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	int passIdx = g_ddofVals.z;
	float2 size = g_ddofVals2.xy;

	//every threadgroup has 2 buffer (not emitting) threads on either end
	int i0 = Gid.x * (PASS0_NUMTHREADS - 2) * 4;

	[unroll]
	for(int i = 0; i < 4; i++)
	{	
		int readIdx = i0 - 4 + PASS0_NUMTHREADS * i;
		int writeIdx = i * PASS0_NUMTHREADS;
		float betaN = betaX(g_depth, int2(GTid.x + readIdx, DTid.y), size);
		float3 color = g_color[int2(GTid.x + readIdx, DTid.y)];
		setc(GTid.x + writeIdx, color);
		p0gs_b[GTid.x + writeIdx] = betaN;
	}
	
	GroupMemoryBarrierWithGroupSync();
	if(GTid.x == 0 || GTid.x == PASS0_NUMTHREADS - 1) return;
	
	float betaLinks[8];
	float3 d[7];
	float lastBeta = p0gs_b[GTid.x * 4 - 1];
	[unroll]
	for(int i = 0; i < 7; i++)
	{
		int gsIdx = GTid.x * 4 + i;
		//GTid.x is 1...n-2 here...
		float betaA = lastBeta;//p0gs_b[gsIdx-1];
		float betaB = p0gs_b[gsIdx];
		betaLinks[i] = min(betaA, betaB);
		d[i] = getc(gsIdx);
		//this only matters on the last loop (other writes will get erased by unroll)
		lastBeta = betaB;
	}

	betaLinks[7] = min(p0gs_b[GTid.x*4+7], lastBeta);	
	
	ABCDEntry abcd[3];	
	[unroll]
	for(int i = 0; i < 3; i++)
	{
		int baseIdx = i * 2;
		ABCDTriple abcd3;
		abcd3.a = float3(-betaLinks[baseIdx + 0], -betaLinks[baseIdx + 1], -betaLinks[baseIdx + 2]);
		abcd3.b = float3(1 + betaLinks[baseIdx + 0] + betaLinks[baseIdx + 1],
			1 + betaLinks[baseIdx + 1] + betaLinks[baseIdx + 2],
			1 + betaLinks[baseIdx + 2] + betaLinks[baseIdx + 3]);
		abcd3.c = float3(-betaLinks[baseIdx + 1], -betaLinks[baseIdx + 2], -betaLinks[baseIdx + 3]);
		abcd3.d = float3x3(d[baseIdx + 0], d[baseIdx + 1], d[baseIdx + 2]);

		abcd[i] = reduce(abcd3);
	}
	ABCDTriple abcd3;
	abcd3.a = float3(abcd[0].a, abcd[1].a, abcd[2].a);
	abcd3.b = float3(abcd[0].b, abcd[1].b, abcd[2].b);
	abcd3.c = float3(abcd[0].c, abcd[1].c, abcd[2].c);
	abcd3.d = float3x3(abcd[0].d, abcd[1].d, abcd[2].d);
	//GTid.x - 1 b/c we had a buffer of 1 thread at beginning
	int2 writeTarget = int2(Gid.x * (PASS0_NUMTHREADS - 2) + GTid.x - 1, DTid.y);
	updateABCD(writeTarget, abcd3);
}
