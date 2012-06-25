
#include "shader.h"
Texture2D<float> g_depth;
Texture2D<float3> g_color;

RWTexture2D<uint4> g_bcd;
cbuffer DDofCB
{
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
uint4 ddofPack(float b, float c, float3 d)
{
	return uint4(
			asuint(f32tof16(d.x) | (f32tof16(d.y) << 16)),
			asuint(d.z),
			asuint(b),
			asuint(c)
		);
}
#define ITERATIONS 1
void ddofPass1(int2 coord0, int2 coordDelta, int numItems)
{	
	float bPrev = 1;
	float cPrev = 0;
	float3 dPrev = 0;
	
	float cocNeg1 = 0;
	float coc0 = z2coc(unproject_z(g_depth[coord0], g_proj_constants), 100, .5, g_ddofVals.x);
	float coc1 = z2coc(unproject_z(g_depth[coord0 + coordDelta], g_proj_constants), 100, .5, g_ddofVals.x);
	float betaNeg1= beta(cocNeg1, int(g_ddofVals.y));
	float beta0 = beta(coc0, int(g_ddofVals.y));
	float beta1 = beta(coc1, int(g_ddofVals.y));

	float betaPrevious = betaNeg1;
	float betaCurrent = beta0;
	float betaNext = beta1;

	for(int i = 0; i < numItems; i++)
	{		
		int2 coordN = coord0 + coordDelta * i;

		float betaBackward = min(betaPrevious, betaCurrent * ((i == 0) ? 0 : 1));
		float betaForward = min(betaCurrent, betaNext * ((i == numItems - 1) ? 0 : 1));
		
		float aCur = -1 * betaBackward;
		float bCur = 1 + betaBackward + betaForward;
		float cCur = -1 * betaForward;

		float m = aCur / bPrev;

		float3 dCur = g_color[coordN];
		float b = bCur - m * cPrev;
		float3 d = (dCur - m * dPrev);

		g_bcd[coordN] = ddofPack(b, cCur, d);

		bPrev = b;
		cPrev = cCur;
		dPrev = d;

		betaPrevious = betaCurrent;
		betaCurrent = betaNext;
		float cocNext = z2coc(unproject_z(g_depth[coordN + 2 * coordDelta], g_proj_constants), 100, .5, g_ddofVals.x);
		betaNext = beta(cocNext, int(g_ddofVals.y));
	}
}
float betaBackward(Texture2D<float> depth, int2 size, int2 xy, int2 forwardXyDelta)
{
	int2 xyPrevious = xy - forwardXyDelta;
	if((xyPrevious.x < 0) || (xyPrevious.y < 0)) return 0;
	float cocPrevious = z2coc(unproject_z(g_depth[xyPrevious], g_proj_constants), 100, .5, g_ddofVals.x);
	float cocCurrent = z2coc(unproject_z(g_depth[xy], g_proj_constants), 100, .5, g_ddofVals.x);
	float betaPrevious = beta(cocPrevious, int(g_ddofVals.y));
	float betaCurrent = beta(cocCurrent, int(g_ddofVals.y));
	return min(betaPrevious, betaCurrent);
}
float betaForward(Texture2D<float> depth, int2 size, int2 xy, int2 forwardXyDelta)
{
	int2 xyNext = xy + forwardXyDelta;
	if((xyNext.x > size.x - 1) || (xyNext.y > size.y - 1)) return 0;
	float cocNext = z2coc(unproject_z(g_depth[xyNext], g_proj_constants), 100, .5, g_ddofVals.x);
	float cocCurrent = z2coc(unproject_z(g_depth[xy], g_proj_constants), 100, .5, g_ddofVals.x);
	float betaNext = beta(cocNext, int(g_ddofVals.y));
	float betaCurrent = beta(cocCurrent, int(g_ddofVals.y));
	return min(betaNext, betaCurrent);
}

[numthreads(32, 1, 1)]
void csPass1H( uint3 DTid : SV_DispatchThreadID )
{
	float2 size;
	//work horizontally.. very inefficient
	g_depth.GetDimensions(size.x, size.y);

	if(DTid.x > (size.y - 1)) return; //we're out of bounds	
	ddofPass1(int2(0, DTid.x), int2(1, 0), size.x);
	return;
}

[numthreads(32, 1, 1)]
void csPass1V( uint3 DTid : SV_DispatchThreadID )
{
	float2 size;
	//work horizontally.. very inefficient
	g_depth.GetDimensions(size.x, size.y);

	if(DTid.x > (size.x - 1)) return; //we're out of bounds	
	ddofPass1(int2(DTid.x, 0), int2(0, 1), size.y);
	return;
}
