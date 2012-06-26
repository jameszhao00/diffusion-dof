#include "DiffusionDof.h"

Texture2D<uint4> g_abcdIn : register(t0);

Texture2D<float> g_depth : register(t1);
Texture2D<float3> g_color : register(t2);

struct DDofOutput
{
	float3 color;
};
RWStructuredBuffer<DDofOutput> g_output : register(u0);

[numthreads(16, 16, 1)]
void csPass2HPassLast(uint3 DTid : SV_DispatchThreadID)
{
	int passIdx = g_ddofVals.z;
	float2 size = g_ddofVals2.xy;

	int i = DTid.x;
	//HACK:
	int2 xy = int2(coordNOffset(DTid.x, passIdx), DTid.y);
	if(xy.x > (size.x - 1)) return; //we're out of bounds	
	if(xy.y > (size.y - 1)) return; //we're out of bounds	
		
	ABCDEntry abcd = ddofUnpack(g_abcdIn[DTid.xy]);

	//0*yA + b*yB + 0*yC = xB ===> yB = xB / b ===> y = x/b			
	g_output[xy.y * size.x + xy.x].color = abcd.d / abcd.b;
}
[numthreads(16, 16, 1)]
void csPass2H(uint3 DTid : SV_DispatchThreadID)
{
	//skip over the entries that are solved in an later passIdx
	int passIdx = g_ddofVals.z;

	float2 size = g_ddofVals2.xy;
	
	//the current to be solved entry is next pass idx's entry shifted up deltaXY at this passIdx
	int2 xyB = int2(coordNOffset(DTid.x, passIdx + 1), DTid.y) 
		- calcXYDelta(int2(1, 0), passIdx);
		
	if(xyB.x > (size.x - 1)) return; //we're out of bounds	

	int2 xyA = xyB - calcXYDelta(int2(1, 0), passIdx);
	int2 xyC = xyB + calcXYDelta(int2(1, 0), passIdx);

	float3 yA = g_output[xyA.y * size.x + xyA.x].color;
	float3 yC = g_output[xyC.y * size.x + xyC.x].color;

	ABCDEntry abcd = ddofUnpack(g_abcdIn[int2(DTid.x * 2, DTid.y)]);
	//ay[n-1] + by[n] + cy[n+1] = d[n]
	//-> y[n] = (d[n] - cy[n+1] - ay[n-1]) / b
	float3 yB = (abcd.d - abcd.c * yC - abcd.a * yA) / abcd.b;
	g_output[xyB.y * size.x + xyB.x].color = yB;
}

//we have to reconstruct a,b,c,d
[numthreads(16, 16, 1)]
void csPass2HFirstPass(uint3 DTid : SV_DispatchThreadID)
{
	//skip over the entries that are solved in an later passIdx
	int passIdx = g_ddofVals.z;

	float2 size = g_ddofVals2.xy;	

	int2 xyB = int2(coordNOffset(DTid.x, 1), DTid.y) 
		- calcXYDelta(int2(1, 0), 0);

	int2 xyAPass0 = xyB - int2(2, 0);
	int2 xyCPass0 = xyB + int2(2, 0);
	ABCDTriple abcd3PassNeg1 = computeABCD(g_depth, g_color, xyB, 
		int2(1, 0), size, g_proj_constants.xy, g_ddofVals.x, g_ddofVals.y);
	ABCDEntry abcdBPass0 = reduce(abcd3PassNeg1);
	float3 yAPass0 = g_output[xyAPass0.y * size.x + xyAPass0.x].color;
	float3 yCPass0 = g_output[xyCPass0.y * size.x + xyCPass0.x].color;
	//for pass = 0
	//a*yA + b*yB + c*yC = d ===> yB = (d - a*yA - c*yC) / b
	float3 yB = 
		(abcdBPass0.d 
		- abcdBPass0.a * yAPass0
		- abcdBPass0.c * yCPass0) / 
		abcdBPass0.b;
	//this line generates errors!
	{
		int outOfBoundsModifier = (xyB.x > size.x - 1) * 10000000;
		g_output[xyB.y * size.x + xyB.x + outOfBoundsModifier].color = yB;
	}

	//fill in stuff for pass = -1
	int2 xyAPassNeg1 = xyAPass0 + int2(1, 0);
	int2 xyCPassNeg1 = xyCPass0 - int2(1, 0);
	
	//for pass = -1
	//a*yA + b*yB + c*yC = d ===> yB = (d - a*yA - c*yC) / b
	{
		int outOfBoundsModifier = (xyAPassNeg1.x > size.x - 1) * 10000000;
		g_output[xyAPassNeg1.y * size.x + xyAPassNeg1.x + outOfBoundsModifier].color = 
			(abcd3PassNeg1.d[0] - abcd3PassNeg1.a[0] * yAPass0 - abcd3PassNeg1.c[0] * yB) / abcd3PassNeg1.b[0];
	}
	{
		int outOfBoundsModifier = (xyAPassNeg1.x > size.x - 1) * 10000000;
		g_output[xyCPassNeg1.y * size.x + xyCPassNeg1.x + outOfBoundsModifier].color = 
			(abcd3PassNeg1.d[2] - abcd3PassNeg1.a[2] * yB - abcd3PassNeg1.c[2] * yCPass0) / abcd3PassNeg1.b[2];
	}
}