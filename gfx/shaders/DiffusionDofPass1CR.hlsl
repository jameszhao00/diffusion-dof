
#include "shader.h"
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
float betaForward(Texture2D<float> depth, int2 size, int2 xy, int2 forwardXyDelta)
{
	int2 xyNext = xy + forwardXyDelta;
	if((xyNext.x > size.x - 1) || (xyNext.y > size.y - 1)) return 0;
	float cocNext = z2coc(unproject_z(g_depth[xyNext], g_proj_constants.xy), 100, .5, g_ddofVals.x);
	float cocCurrent = z2coc(unproject_z(g_depth[xy], g_proj_constants.xy), 100, .5, g_ddofVals.x);
	float betaNext = beta(cocNext, int(g_ddofVals.y));
	float betaCurrent = beta(cocCurrent, int(g_ddofVals.y));
	return min(betaNext, betaCurrent);
}
void updateABCD(int2 xy, ABCD abcd)
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
ABCD computeABCD(int2 xy, int2 xyDelta, int2 size)
{
	ABCD abcd;
	//TODO: do boundary testing
	float betaA = betaForward(g_depth, size, xy - 2 * xyDelta, xyDelta);
	//n-1 <-> n
	float betaB = betaForward(g_depth, size, xy - xyDelta, xyDelta);
	//n <-> n+1
	float betaC = betaForward(g_depth, size, xy, xyDelta);
	//n+1 <-> n+2
	float betaD = betaForward(g_depth, size, xy + xyDelta, xyDelta);
	
	abcd.a = float3(-betaA, -betaB, -betaC);
	abcd.b = float3(1 + betaA + betaB, 1 + betaB + betaC, 1 + betaC + betaD);
	abcd.c = float3(-betaB, -betaC, -betaD);
	abcd.d = float3x3(g_color[xy - xyDelta], g_color[xy], g_color[xy + xyDelta]);

	return abcd;
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

	ABCD abcd = readABCD(g_abcdIn, xy, calcXYDelta(int2(1, 0), passIdx));
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

	ABCD abcd = computeABCD(xy, int2(1, 0), size);
	updateABCD(xy, abcd);
}
