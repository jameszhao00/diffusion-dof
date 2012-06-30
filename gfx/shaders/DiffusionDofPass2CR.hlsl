#include "DiffusionDof.h"

Texture2D<uint4> g_abcdIn : register(t0);

Texture2D<float> g_depth : register(t1);
Texture2D<float3> g_color : register(t2);

Texture2D<float3> g_yIn : register(t3);
RWTexture2D<float3> g_yOut : register(u0);

[numthreads(16, 16, 1)]
void csPass2HPassLast(uint3 DTid : SV_DispatchThreadID)
{
	int passIdx = g_ddofVals.z;
	float2 size = g_ddofVals2.xy;

	int i = DTid.x;
	//TODO: do we need this? seems xy is unused
	int2 xy = int2(coordNOffset(DTid.x, passIdx), DTid.y);
	if(xy.x > (size.x - 1)) return; //we're out of bounds	
	if(xy.y > (size.y - 1)) return; //we're out of bounds	
		
	ABCDEntry abcd = ddofUnpack(g_abcdIn[DTid.xy]);

	//0*yA + b*yB + 0*yC = xB ===> yB = xB / b ===> y = x/b			
	g_yOut[int2(0, DTid.y)] = abcd.d / abcd.b;
}
[numthreads(16, 16, 1)]
void csPass2H(uint3 DTid : SV_DispatchThreadID)
{
	//skip over the entries that are solved in an later passIdx
	int passIdx = g_ddofVals.z;

	float2 size = g_ddofVals2.xy;	

	float3 yA = g_yIn[DTid.xy - int2(1, 0)];
	float3 yC = g_yIn[DTid.xy];

	ABCDEntry abcd = ddofUnpack(g_abcdIn[int2(DTid.x * 2, DTid.y)]);
	//ay[n-1] + by[n] + cy[n+1] = d[n]
	//-> y[n] = (d[n] - cy[n+1] - ay[n-1]) / b
	float3 yB = (abcd.d - abcd.c * yC - abcd.a * yA) / abcd.b;
	g_yOut[int2(DTid.x * 2 - 1, DTid.y)] = yA;
	g_yOut[int2(DTid.x * 2, DTid.y)] = yB;
}

#define PASS0_NUMTHREADS 64
groupshared uint2 pass0_gsmem[PASS0_NUMTHREADS * 4]; //for depth,color
//we have to reconstruct a,b,c,d
[numthreads(PASS0_NUMTHREADS, 1, 1)]
void csPass2HFirstPass(uint3 Gid : SV_GroupId, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	//skip over the entries that are solved in an later passIdx
	int passIdx = g_ddofVals.z;
	float2 size = g_ddofVals2.xy;

	int i0 = Gid.x * (PASS0_NUMTHREADS - 1) * 4 - 2; //starts after the previous entry
	[unroll]
	for(int i = 0; i < 4; i++)
	{	
		int readIdx = i0 + PASS0_NUMTHREADS * i;
		int writeIdx = i * PASS0_NUMTHREADS;
		float betaN = betaX(g_depth, int2(GTid.x + readIdx, DTid.y), size);
		pass0_gsmem[GTid.x + writeIdx] = packf4(float4(g_color[int2(GTid.x + readIdx, DTid.y)], betaN));
	}
	
	GroupMemoryBarrierWithGroupSync();
	//buffer thread at the end of the group
	if(GTid.x == PASS0_NUMTHREADS - 1) return;
	
	//pass0_gsmem[0] is 1 before what equation 1 is really made of!

	float betaLinks[4];
	float3 d[3];
	[unroll]
	for(int i = 0; i < 3; i++)
	{		
		float4 cdA = unpackf4(pass0_gsmem[GTid.x * 4 + i + 1]);
		float4 cdB = unpackf4(pass0_gsmem[GTid.x * 4 + i + 2]);

		betaLinks[i] = min(cdB.w, cdA.w);
		d[i] = cdB.xyz;
	}
	betaLinks[3] = min(unpackf4(pass0_gsmem[GTid.x * 4 + 3 + 2]).w, unpackf4(pass0_gsmem[GTid.x * 4 + 3 + 1]).w);

	ABCDTriple abcd3PassNeg1;
	abcd3PassNeg1.a = float3(-betaLinks[0], -betaLinks[1], -betaLinks[2]);
	abcd3PassNeg1.b = float3(1+betaLinks[0]+betaLinks[1], 1+betaLinks[1]+betaLinks[2], 1+betaLinks[2]+betaLinks[3]);
	abcd3PassNeg1.c = float3(-betaLinks[1], -betaLinks[2], -betaLinks[3]);
	abcd3PassNeg1.d = float3x3(d[0], d[1], d[2]);

	ABCDEntry abcdBPass0 = reduce(abcd3PassNeg1);


	
	float3 yAPass0 = g_yIn[int2(Gid.x * (PASS0_NUMTHREADS - 1) + GTid.x - 1, DTid.y)];
	float3 yCPass0 = g_yIn[int2(Gid.x * (PASS0_NUMTHREADS - 1) + GTid.x, DTid.y)];
	float3 yB = (abcdBPass0.d - abcdBPass0.a * yAPass0 - abcdBPass0.c * yCPass0) / abcdBPass0.b;
	int2 xyB = int2(Gid.x * (PASS0_NUMTHREADS - 1) * 4 + GTid.x * 4 + 1, DTid.y);
	g_yOut[xyB] = yB;
	g_yOut[xyB - int2(2, 0)] = yAPass0;	


	//for pass = -1
	//a*yA + b*yB + c*yC = d ===> yB = (d - a*yA - c*yC) / b	
	g_yOut[xyB - int2(1, 0)] = 
		(abcd3PassNeg1.d[0] - abcd3PassNeg1.a[0] * yAPass0 - abcd3PassNeg1.c[0] * yB) / abcd3PassNeg1.b[0];
	
	g_yOut[xyB + int2(1, 0)] = 
		(abcd3PassNeg1.d[2] - abcd3PassNeg1.a[2] * yB - abcd3PassNeg1.c[2] * yCPass0) / abcd3PassNeg1.b[2];
	
}