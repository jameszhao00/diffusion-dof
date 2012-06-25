#include "DiffusionDof.h"
Texture2D<float> g_depth : register(t0);
Texture2D<float3> g_color : register(t1);
//TODO: out of bound reads should return 0! required for proper m[] behavior
Texture2D<uint4> g_abcdIn : register(t2);
RWTexture2D<uint4> g_abcdOut : register(u0);
//use odd entries
cbuffer DDofCB
{
	//x = focal plane
	//y = # iterations
	//z = passidx
	float4 g_ddofVals;
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
	float m[] = {
		abcd.b[0] == 0 ? 0 : (abcd.a[1] / abcd.b[0]),
		abcd.b[2] == 0 ? 0 : (abcd.c[1] / abcd.b[2])
	};

	float a = -m[0] * abcd.a[0];
	float b = abcd.b[1] - m[0] * abcd.c[0] - m[1] * abcd.a[2];
	float c = -m[1] * abcd.c[2];
	//todo: take care of endpoints!
	//i think it does at the moment, but confirm
	float3 d = abcd.d[1] - m[0] * abcd.d[0] - m[1] * abcd.d[2];
	g_abcdOut[xy] = ddofPack(a, b, c, d);
}
[numthreads(16, 16, 1)]
void csPass1H(uint3 DTid : SV_DispatchThreadID)
{
	int passIdx = g_ddofVals.z;
	float2 size;
	g_depth.GetDimensions(size.x, size.y);
	int i = DTid.x;

	int2 xy = int2(coordNOffset(DTid.x, passIdx), DTid.y);
	if(xy.x > (size.x - 1)) return; //we're out of bounds	
	if(xy.y > (size.y - 1)) return; //we're out of bounds	
	//calcXYDelta - we're looking for spacing at passIdx - 1
	ABCDTriple abcd = readABCD(g_abcdIn, xy, calcXYDelta(int2(1, 0), passIdx - 1));
	updateABCD(xy, abcd);
}
[numthreads(16, 16, 1)]
void csPass1HPass0(uint3 DTid : SV_DispatchThreadID)
{
	int passIdx = g_ddofVals.z;
	float2 size;
	g_depth.GetDimensions(size.x, size.y);
	int i = DTid.x;

	int2 xy = int2(coordNOffset(DTid.x, 0), DTid.y);
	if(xy.x > (size.x - 1)) return; //we're out of bounds	
	if(xy.y > (size.y - 1)) return; //we're out of bounds	

	ABCDTriple abcd = computeABCD(g_depth, g_color, xy, int2(1, 0), size, g_proj_constants.xy, g_ddofVals.x, g_ddofVals.y);
	updateABCD(xy, abcd);
}
