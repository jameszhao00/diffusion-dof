#include "DiffusionDof.h"
Texture2D<float> g_depth : register(t0);
Texture2D<float3> g_color : register(t1);

Texture2D<uint4> g_abcdIn : register(t2);
RWTexture2D<uint4> g_abcdOut : register(u0);
//use odd entries
cbuffer DDofCB
{
	//x = focal plane
	//y = # iterations
	//z = passidx
	float4 g_ddofVals;
	//x,y = image width/height
	float4 g_ddofVals2;
};
cbuffer FSQuadCB : register(b1)
{	
	float4x4 g_inv_p;
	float4 g_proj_constants;
	float4 g_debug_vars;
	float4x4 g_proj;
};
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
#define H_GROUPSIZE 16
#define V_GROUPSIZE 16
[numthreads(H_GROUPSIZE, V_GROUPSIZE, 1)]
void csPass1H(uint3 DTid : SV_DispatchThreadID)
{
	int passIdx = g_ddofVals.z;
	float2 size = g_ddofVals2.xy;	
	int2 xy = int2(2 * DTid.x + 1, DTid.y);

	ABCDTriple abcd = readABCD(g_abcdIn, xy, int2(1, 0));
	updateABCD(DTid.xy, abcd);
}

[numthreads(H_GROUPSIZE, V_GROUPSIZE, 1)]
void csPass1HPass0(uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	int passIdx = g_ddofVals.z;
	float2 size = g_ddofVals2.xy;

	//int2 xy = int2(2 * DTid.x + 1, DTid.y);

	//ABCDTriple abcd = computeABCD(g_depth, g_color, xy, 
	//		int2(1, 0), size, g_proj_constants.xy, g_ddofVals.x, g_ddofVals.y);
	//updateABCD(DTid.xy, abcd);
	
	int2 xy = int2(4 * DTid.x + 3, DTid.y);
	ABCDEntry abcdA = reduce(computeABCD(g_depth, g_color, xy - int2(2, 0), 
		int2(1, 0), size, g_proj_constants.xy, g_ddofVals.x, g_ddofVals.y));
	
	ABCDEntry abcdB = reduce(computeABCD(g_depth, g_color, xy, 
		int2(1, 0), size, g_proj_constants.xy, g_ddofVals.x, g_ddofVals.y));
	
	ABCDEntry abcdC = reduce(computeABCD(g_depth, g_color, xy + int2(2, 0), 
		int2(1, 0), size, g_proj_constants.xy, g_ddofVals.x, g_ddofVals.y));

	ABCDTriple abcd3;
	abcd3.a = float3(abcdA.a, abcdB.a, abcdC.a);
	abcd3.b = float3(abcdA.b, abcdB.b, abcdC.b);
	abcd3.c = float3(abcdA.c, abcdB.c, abcdC.c);
	abcd3.d = float3x3(abcdA.d, abcdB.d, abcdC.d);

	updateABCD(DTid.xy, abcd3);
}
