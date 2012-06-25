#include "DiffusionDof.h"

Texture2D<uint4> g_abcdIn : register(t0);

Texture2D<float> g_depth : register(t1);
Texture2D<float3> g_color : register(t2);

struct DDofOutput
{
	float3 color;
};
RWStructuredBuffer<DDofOutput> g_output : register(u0);

cbuffer DDofCB
{
	//x = focal plane
	//z = passidx
	//w = debug pass idx
	float4 g_ddofVals;
};
cbuffer FSQuadCB : register(b1)
{	
	float4x4 g_inv_p;
	float4 g_proj_constants;
	float4 g_debug_vars;
	float4x4 g_proj;
};
[numthreads(16, 16, 1)]
void csPass2HPassLast(uint3 DTid : SV_DispatchThreadID)
{
	int passIdx = g_ddofVals.z;
	float2 size;
	g_abcdIn.GetDimensions(size.x, size.y);
	int i = DTid.x;
	//HACK:
	int2 xy = int2(coordNOffset(DTid.x, passIdx), DTid.y);
	if(xy.x > (size.x - 1)) return; //we're out of bounds	
	if(xy.y > (size.y - 1)) return; //we're out of bounds	
		
	ABCDEntry abcd = ddofUnpack(g_abcdIn[xy]);
	float3 y = abcd.d / abcd.b;
			
	g_output[xy.y * size.x + xy.x].color = y.xyzz;
	//HACK:
	//g_output[xy.y * size.x + xy.x].color = (passIdx == g_ddofVals.w) * y.xyzz;

}
[numthreads(16, 16, 1)]
void csPass2H(uint3 DTid : SV_DispatchThreadID)
{
	//skip over the entries that are solved in an later passIdx

	int passIdx = g_ddofVals.z;

	float2 size;
	g_abcdIn.GetDimensions(size.x, size.y);
	
	//the current to be solved entry is next pass idx's entry shifted up deltaXY at this passIdx
	int2 xyB = int2(coordNOffset(DTid.x, passIdx + 1), DTid.y) - calcXYDelta(int2(1, 0), passIdx);
		
	if(xyB.x > (size.x - 1)) return; //we're out of bounds	
	if(xyB.y > (size.y - 1)) return; //we're out of bounds	

	int2 xyA = xyB - calcXYDelta(int2(1, 0), passIdx);
	int2 xyC = xyB + calcXYDelta(int2(1, 0), passIdx);

	float3 yA = g_output[xyA.y * size.x + xyA.x].color;
	float3 yC = g_output[xyC.y * size.x + xyC.x].color;
	ABCDEntry abcd = ddofUnpack(g_abcdIn[xyB]);
	//ay[n-1] + by[n] + cy[n+1] = d[n]
	//-> y[n] = (d[n] - cy[n+1] - ay[n-1]) / b
	float3 yB = (abcd.d - abcd.c * yC - abcd.a * yA) / abcd.b;
	g_output[xyB.y * size.x + xyB.x].color = yB;
	//HACK:
	//g_output[xyB.y * size.x + xyB.x].color = (passIdx == g_ddofVals.w) * yB.xyzz;
}

//we have to reconstruct a,b,c,d
[numthreads(16, 16, 1)]
void csPass2HFirstPass(uint3 DTid : SV_DispatchThreadID)
{

	//skip over the entries that are solved in an later passIdx

	int passIdx = g_ddofVals.z;

	float2 size;
	g_abcdIn.GetDimensions(size.x, size.y);
	
	//the current to be solved entry is next pass idx's entry shifted up deltaXY at this passIdx
	int2 xyB = int2(coordNOffset(DTid.x, passIdx + 1), DTid.y) - calcXYDelta(int2(1, 0), passIdx);
		
	if(xyB.x > (size.x - 1)) return; //we're out of bounds	
	if(xyB.y > (size.y - 1)) return; //we're out of bounds	

	int2 xyA = xyB - calcXYDelta(int2(1, 0), passIdx);
	int2 xyC = xyB + calcXYDelta(int2(1, 0), passIdx);
	
	ABCD abcd = computeABCD(g_depth, g_color, xyB, int2(1, 0), size, g_proj_constants.xy, g_ddofVals.x, g_ddofVals.y);
	
	float3 yA = g_output[xyA.y * size.x + xyA.x].color;
	float3 yC = g_output[xyC.y * size.x + xyC.x].color;
	
	//ay[n-1] + by[n] + cy[n+1] = d[n]
	//-> y[n] = (d[n] - cy[n+1] - ay[n-1]) / b
	float3 yB = (abcd.d[1] - abcd.c[1] * yC - abcd.a[1] * yA) / abcd.b[1];
	g_output[xyB.y * size.x + xyB.x].color = yB;
	//HACK:
	//g_output[xyB.y * size.x + xyB.x].color = (passIdx == g_ddofVals.w) * yB.xyzz;
}