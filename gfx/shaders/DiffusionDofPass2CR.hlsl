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
//bitmask for 11/10 low bits
const static int X11 = 0xFFE0;
const static int X10 = 0xFFC0;

//r11g11b10 packing
uint packf3(float3 v)
{
	return (
		((f32tof16(v.x) & X11) << 16) |
		((f32tof16(v.y) & X11) << 5) |
		((f32tof16(v.z) & X10) >> 6)
		);
}
float3 unpackf3(uint v)
{
	return float3(
		f16tof32((v >> 16) & X11),
		f16tof32((v >> 5) & X11),
		f16tof32((v << 6) & X10));
}
#define PASS0_X_SIZE 8
#define PASS0_Y_SIZE 16
groupshared uint pass0_gsmem[PASS0_Y_SIZE][PASS0_X_SIZE*4]; //for depth,color
//we have to reconstruct a,b,c,d
[numthreads(PASS0_X_SIZE, PASS0_Y_SIZE, 1)]
void csPass2HFirstPass(uint3 Gid : SV_GroupId, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	
	//skip over the entries that are solved in an later passIdx
	int passIdx = g_ddofVals.z;

	float2 size = g_ddofVals2.xy;	

	int2 xyB = int2(coordNOffset(DTid.x, 1), DTid.y) - calcXYDelta(int2(1, 0), 0);

	ABCDTriple abcd3PassNeg1 = computeABCD(g_depth, g_color, xyB, 
		int2(1, 0), size, g_proj_constants.xy, g_ddofVals.x, g_ddofVals.y);

	ABCDEntry abcdBPass0 = reduce(abcd3PassNeg1);
	
	float3 yAPass0 = g_yIn[DTid.xy - int2(1, 0)];
	float3 yCPass0 = g_yIn[DTid.xy];
	//for pass = 0
	//a*yA + b*yB + c*yC = d ===> yB = (d - a*yA - c*yC) / b
	float3 yB = (abcdBPass0.d - abcdBPass0.a * yAPass0 - abcdBPass0.c * yCPass0) / abcdBPass0.b;

	pass0_gsmem[GTid.y][GTid.x * 4 + 0] = packf3(yAPass0);
	pass0_gsmem[GTid.y][GTid.x * 4 + 1] = packf3(
		(abcd3PassNeg1.d[0] - abcd3PassNeg1.a[0] * yAPass0 - abcd3PassNeg1.c[0] * yB) / abcd3PassNeg1.b[0]);
	pass0_gsmem[GTid.y][GTid.x * 4 + 2] = packf3(yB);
	pass0_gsmem[GTid.y][GTid.x * 4 + 3] = packf3(
		(abcd3PassNeg1.d[2] - abcd3PassNeg1.a[2] * yB - abcd3PassNeg1.c[2] * yCPass0) / abcd3PassNeg1.b[2]);

	GroupMemoryBarrierWithGroupSync();
	//starts writing at xyB(0) - int2(2, 0)
	int2 xy0 = int2(4 * Gid.x * PASS0_X_SIZE - 1, Gid.y * PASS0_Y_SIZE);
	//successively write blocks to the right
	for(int i = 0; i < 4; i++)
	{
		int2 outXY = xy0 + int2(i * PASS0_X_SIZE + GTid.x, GTid.y);
		int2 inIJ = int2(i * PASS0_X_SIZE + GTid.x, GTid.y);
		//write
		g_yOut[outXY] = unpackf3(pass0_gsmem[inIJ.y][inIJ.x]);
	}
}