#include "DiffusionDof.h"

Texture2D<uint4> g_abcdIn : register(t0);

Texture2D<float> g_depth : register(t1);
Texture2D<float3> g_color : register(t2);

Texture2D<float3> g_yIn : register(t3);
RWTexture2D<float3> g_yOut : register(u0);

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
#define PASS0_X_SIZE 16
#define PASS0_Y_SIZE 16

groupshared float pass0_gsmem[3][PASS0_Y_SIZE][PASS0_X_SIZE*4]; //for depth,color

void setc(int2 ij, float3 c)
{
	pass0_gsmem[0][ij.x][ij.y] = c.x;
	pass0_gsmem[1][ij.x][ij.y] = c.y;
	pass0_gsmem[2][ij.x][ij.y] = c.z;
}
float3 getc(int2 ij)
{
	return float3(pass0_gsmem[0][ij.x][ij.y], pass0_gsmem[1][ij.x][ij.y], pass0_gsmem[2][ij.x][ij.y]);
}
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
	
	setc(int2(GTid.y, GTid.x * 4 + 0), yAPass0);
	setc(int2(GTid.y, GTid.x * 4 + 1), 
		(abcd3PassNeg1.d[0] - abcd3PassNeg1.a[0] * yAPass0 - abcd3PassNeg1.c[0] * yB) / abcd3PassNeg1.b[0]);
	setc(int2(GTid.y, GTid.x * 4 + 2), yB);
	setc(int2(GTid.y, GTid.x * 4 + 3), 
		(abcd3PassNeg1.d[2] - abcd3PassNeg1.a[2] * yB - abcd3PassNeg1.c[2] * yCPass0) / abcd3PassNeg1.b[2]);
	GroupMemoryBarrierWithGroupSync();
	//starts writing at xyB(0) - int2(2, 0)
	int2 xy0 = int2(4 * Gid.x * PASS0_X_SIZE - 1, Gid.y * PASS0_Y_SIZE);
	//successively write blocks to the right
	[unroll]
	for(int i = 0; i < 4; i++)
	{
		int2 outXY = xy0 + int2(i * PASS0_X_SIZE + GTid.x, GTid.y);
		int2 inIJ = int2(i * PASS0_X_SIZE + GTid.x, GTid.y);
		//write
		g_yOut[outXY] = getc(inIJ.yx);
	}
}