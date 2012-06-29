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
//MUST BE DIVISIBLE BY 4
#define H_GROUPSIZE 16
#define V_GROUPSIZE 16

[numthreads(H_GROUPSIZE, V_GROUPSIZE, 1)]
void csPass1H(uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	int passIdx = g_ddofVals.z;
	float2 size = g_ddofVals2.xy;	
	int2 xy = int2(2 * DTid.x + 1, DTid.y);
	ABCDTriple abcd = readABCD(g_abcdIn, xy, int2(1, 0));		
	
	updateABCD(DTid.xy, abcd);
}

#define PASS0_NUMTHREADS_X 8
#define PASS0_NUMTHREADS_Y 8
groupshared float2 depthcolor_gsmem[PASS0_NUMTHREADS_Y / 4 * PASS0_NUMTHREADS_X];

[numthreads(PASS0_NUMTHREADS_X, PASS0_NUMTHREADS_Y, 1)]
void csPass1HPass0(uint3 Gid : SV_GroupId, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	int passIdx = g_ddofVals.z;
	float2 size = g_ddofVals2.xy;

	int2 xy = int2(4 * DTid.x + 3, DTid.y);

	//compute abcd for x - 3, to x + 3
	float2 projConstants = g_proj_constants.xy;
	float focalPlane = g_ddofVals.x;
	float iterations = g_ddofVals.y;
	//compute thread group's starting x in the original image
	/*
	int i0 = Gid.x * H_GROUPSIZE * 4 + GTid.x;
	[unroll]
	for(int i = 0; i < PASS0_NUMTHREADS_Y / 4; i++)
	{			
		int2 threadReadIdx0 = (PASS0_NUMTHREADS_Y / 4) * i;
	}
	//coordinated read a block's depth/color
	//compute + store beta and min(betaN, betaN+1) and same for backwards
	//copy to local variable on relevant threads

	//read 1st batch [i0, i0 + GROUPSIZE) of color / beta into gsmem	
	
	//g_depth[int2(i0 + GTid.x, GTid.x ]
	GroupMemoryBarrierWithGroupSync();
	//read 2nd batch [iHalfN, iHalfN + GROUPSIZE) of color / beta into gsmem
	int iHalfN = (Gid.x + 1) * (H_GROUPSIZE / 2) * 4;
	GroupMemoryBarrierWithGroupSync();
	//compute my own abcd
	//optionally compute 0th abcd
	*/
	
	float betaPrev = betaX(g_depth, xy - int2(3, 0) - int2(1, 0), size);
	float betaCur = betaX(g_depth, xy - int2(3, 0), size);
	float betaNext = 0;
	float prevC = -min(betaPrev, betaCur);
	ABCDTriple abcd;
	int2 xyDelta = int2(1, 0);
	[unroll]
	for(int i = -3; i < 0; i++)
	{		
		betaNext = betaX(g_depth, xy + int2(i + 1, 0), size);
		float betaForward = min(betaCur, betaNext);

		int idx = i+3;
		
		abcd.a[idx] = prevC;
		abcd.b[idx] = 1 - prevC + betaForward;
		float c = -betaForward;
		abcd.c[idx] = c;
		abcd.d[idx] = g_color[xy + i * xyDelta];
		prevC = c;
		betaPrev = betaCur;
		betaCur = betaNext;		
	}
	ABCDEntry abcdA = reduce(abcd);
	abcd.a[0] = abcd.a[2];
	abcd.b[0] = abcd.b[2];
	abcd.c[0] = abcd.c[2];
	abcd.d[0] = abcd.d[2];
	[unroll]
	for(int i = 0; i < 2; i++)
	{		
		betaNext = betaX(g_depth, xy + int2(i + 1, 0), size);
		float betaForward = min(betaCur, betaNext);

		//map i=0 to x[1] (we've already computed x[0])
		int idx = i+1;
		
		abcd.a[idx] = prevC;
		abcd.b[idx] = 1 - prevC + betaForward;
		float c = -betaForward;
		abcd.c[idx] = c;
		abcd.d[idx] = g_color[xy + i * xyDelta];
		prevC = c;
		betaPrev = betaCur;
		betaCur = betaNext;		
	}
	ABCDEntry abcdB = reduce(abcd);	
	abcd.a[0] = abcd.a[2];
	abcd.b[0] = abcd.b[2];
	abcd.c[0] = abcd.c[2];
	abcd.d[0] = abcd.d[2];
	[unroll]
	for(int i = 2; i < 4; i++)
	{		
		betaNext = betaX(g_depth, xy + int2(i + 1, 0), size);
		float betaForward = min(betaCur, betaNext);

		//map i=2 to x[1] (we've already computed x[0])
		int idx = i - 1;
		
		abcd.a[idx] = prevC;
		abcd.b[idx] = 1 - prevC + betaForward;
		float c = -betaForward;
		abcd.c[idx] = c;
		abcd.d[idx] = g_color[xy + i * xyDelta];
		prevC = c;
		betaPrev = betaCur;
		betaCur = betaNext;		
	}
	ABCDEntry abcdC = reduce(abcd);

	ABCDTriple abcd3;
	abcd3.a = float3(abcdA.a, abcdB.a, abcdC.a);
	abcd3.b = float3(abcdA.b, abcdB.b, abcdC.b);
	abcd3.c = float3(abcdA.c, abcdB.c, abcdC.c);
	abcd3.d = float3x3(abcdA.d, abcdB.d, abcdC.d);

	updateABCD(DTid.xy, abcd3);
	
}
