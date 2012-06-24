#include "DiffusionDof.h"

Texture2D<uint4> g_abcdIn : register(t0);
//for reading b during the last pass
Texture2D<uint4> g_abcdInPassLast : register(t1);

struct DDofOutput
{
	float3 color;
};
RWStructuredBuffer<DDofOutput> g_output : register(u0);

cbuffer DDofCB
{
	//x = focal plane
	//z = passidx
	float4 g_ddofVals;
};

[numthreads(16, 16, 1)]
void csPass2HPassLast(uint3 DTid : SV_DispatchThreadID)
{
	int passIdx = g_ddofVals.z;
	float2 size;
	g_abcdIn.GetDimensions(size.x, size.y);
	int i = DTid.x;

	int2 xy = int2(coordNOffset(DTid.x, passIdx), DTid.y);
	if(xy.x > (size.x - 1)) return; //we're out of bounds	
	if(xy.y > (size.y - 1)) return; //we're out of bounds	

	//only b should be non-zero
	float3 y = 0;
	{
		float a = 0, b = 0, c = 0; 
		float3 d = 0;
		ddofUnpack(g_abcdInPassLast[xy], a, b, c, d);
		float3 y = d / b;
		g_output[xy.y * size.x + xy.x].color = y;
	}
	//substitute y into the 2 other equations

	int2 xyDelta = calcXYDelta(int2(1, 0), passIdx - 1);
	//substitute y into the 2 other equations at passIdx - 1
	//float3 k = g_solvedOutput[int2(1, 0)];
	{
		
		float a = 0, b = 0, c = 0; 
		float3 d = 0;

		int2 xyPrevious = xy - xyDelta;
		ddofUnpack(g_abcdIn[xyPrevious], a, b, c, d);
		//b*x0 + c*x1 = d, (d - c*x1) / b = x0
		g_output[xyPrevious.y * size.x + xyPrevious.x].color = (d - c * y) / b;
	}
	{		
		float a = 0, b = 0, c = 0; 
		float3 d = 0;

		int2 xyNext = xy + xyDelta;
		ddofUnpack(g_abcdIn[xyNext], a, b, c, d);
		//a*x1 + b*x2 = d, (d - a * x1) / b = x2
		g_output[xyNext.y * size.x + xyNext.x].color = (d - a * y) / b;
	}
}
[numthreads(16, 16, 1)]
void csPass2H(uint3 DTid : SV_DispatchThreadID)
{
	int passIdx = g_ddofVals.z;
	float2 size;
	g_abcdIn.GetDimensions(size.x, size.y);
	int i = DTid.x;

	int2 xy = int2(coordNOffset(DTid.x, passIdx), DTid.y);
	if(xy.x > (size.x - 1)) return; //we're out of bounds	
	if(xy.y > (size.y - 1)) return; //we're out of bounds	
	//solve for b instead of reading it if we're just starting the substitution phase
	ABCD abcd = readABCD(g_abcdIn, xy, calcXYDelta(int2(1, 0), passIdx));

	//read y for xy
	float3 y = g_output[xy.y * size.x + xy.x].color;
	int2 xyDelta = calcXYDelta(int2(1, 0), passIdx - 1);
	//substitute y into the 2 other equations at passIdx - 1
	//float3 k = g_solvedOutput[int2(1, 0)];
	{
		
		float a = 0, b = 0, c = 0; 
		float3 d = 0;

		int2 xyPrevious = xy - xyDelta;
		ddofUnpack(g_abcdIn[xyPrevious], a, b, c, d);
		//b*x0 + c*x1 = d, (d - c*x1) / b = x0
		g_output[xyPrevious.y * size.x + xyPrevious.x].color = (d - c * y) / b;
		
	}
	{		
		float a = 0, b = 0, c = 0; 
		float3 d = 0;

		int2 xyNext = xy + xyDelta;
		ddofUnpack(g_abcdIn[xyNext], a, b, c, d);
		//a*x1 + b*x2 = d, (d - a * x1) / b = x2
		g_output[xyNext.y * size.x + xyNext.x].color = (d - a * y) / b;
	}
}