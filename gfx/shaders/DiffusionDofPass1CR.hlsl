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
groupshared uint2 pass0_gsmem[PASS0_NUMTHREADS * 4]; //for depth,color

[numthreads(PASS0_NUMTHREADS, 1, 1)]
void csPass1HPass0(uint3 Gid : SV_GroupId, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	int passIdx = g_ddofVals.z;
	float2 size = g_ddofVals2.xy;

	//every threadgroup has 2 buffer threads on either end
	int i0 = Gid.x * (PASS0_NUMTHREADS - 2) * 4; //3 elements before

	[unroll]
	for(int i = 0; i < 4; i++)
	{	
		int readIdx = i0 - 4 + PASS0_NUMTHREADS * i;
		int writeIdx = i * PASS0_NUMTHREADS;
		float betaN = betaX(g_depth, int2(GTid.x + readIdx, DTid.y), size);
		pass0_gsmem[GTid.x + writeIdx] = packf4(float4(g_color[int2(GTid.x + readIdx, DTid.y)], betaN));
	}
	
	GroupMemoryBarrierWithGroupSync();
	if(GTid.x == 0 || GTid.x == PASS0_NUMTHREADS - 1) return;
	
	float betaLinks[8];
	float3 d[7];
	[unroll]
	for(int i = 0; i < 7; i++)
	{
		//GTid.x is 1...n-2 here...
		float4 cdB = unpackf4(pass0_gsmem[GTid.x * 4 + i]);
		float4 cdA = unpackf4(pass0_gsmem[GTid.x * 4 + i - 1]);
		betaLinks[i] = min(cdB.w, cdA.w);
		d[i] = cdB.xyz;
	}
	betaLinks[7] = min(unpackf4(pass0_gsmem[GTid.x * 4 + 7]).w, unpackf4(pass0_gsmem[GTid.x * 4 + 6]).w);	
	
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
	updateABCD(int2(Gid.x * (PASS0_NUMTHREADS - 2) + GTid.x - 1, DTid.y), abcd3);

}
