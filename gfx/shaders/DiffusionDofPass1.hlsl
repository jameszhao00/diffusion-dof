#include "shader.h"
Texture2D<float> g_depth;
Texture2D<float3> g_color;

RWTexture2D<float3> g_bc;
RWTexture2D<float3> g_d;

cbuffer DDofCB
{
	float4 g_ddofVals;
};
cbuffer FSQuadCB : register(b1)
{	
	float4x4 g_inv_p;
	float4 g_proj_constants;
	float4 g_debug_vars;
	float4x4 g_proj;
};
#define ITERATIONS 1
/*
void ddofPass1(int2 coord0, int2 coordDelta, int numItems)
{	
	float betaPrevious = 0;

	float cocCurrent = z2coc(unproject_z(g_depth[coord0], g_proj_constants), 100, .5, g_ddofVals.x);
	float betaCurrent = beta(cocCurrent, ITERATIONS);
	
	float cocNext = z2coc(unproject_z(g_depth[coord0 + coordDelta], g_proj_constants), 100, .5, g_ddofVals.x);	
	float betaNext = beta(cocNext, ITERATIONS);
	
	float betaBackward = 1;
	float betaForward = min(betaCurrent, betaNext);
	
	float bPrev = 1 + betaBackward + betaForward;
	float cPrev = -betaForward;
	float3 dPrev = g_color[coord0];

	g_bc[coord0] = float3(0, bPrev, cPrev);
	g_d[coord0] = dPrev;
	
	for(int i = 1; i < numItems; i++)
	{		
		int2 coordN = coord0 + coordDelta * i;
		betaPrevious = betaCurrent;
		betaCurrent = betaNext;
		
		float cocNext = z2coc(unproject_z(g_depth[coordN + coordDelta], g_proj_constants), 100, .5, g_ddofVals.x);		
		betaNext = beta(cocNext, ITERATIONS);

		float betaBackward = min(betaPrevious, betaCurrent);
		float betaForward = min(betaCurrent, betaNext * ((i == numItems - 1) ? 0 : 1));
		
		float aCur = -1 * betaBackward;
		float bCur = 1 + betaBackward + betaForward;
		float cCur = -1 * betaForward;
		float m = aCur / bPrev;

		float3 dCur = g_color[coordN];
		float b = bCur - m * cPrev;
		g_bc[coordN] = float3(0, b, cCur);
		float3 d = (dCur - m * dPrev);

		g_d[coordN] = d;
		bPrev = b;
		cPrev = cCur;
		dPrev = d;
	}
}
*/
void ddofPass1(int2 coord0, int2 coordDelta, int numItems)
{	
	float bPrev = 1;
	float cPrev = 0;
	float3 dPrev = 0;
	

	for(int i = 0; i < numItems; i++)
	{		
		int2 coordN = coord0 + coordDelta * i;
		
		float cocNext = z2coc(unproject_z(g_depth[coordN + coordDelta], g_proj_constants), 100, .5, g_ddofVals.x);
		float cocCurrent = z2coc(unproject_z(g_depth[coordN], g_proj_constants), 100, .5, g_ddofVals.x);
		float cocPrevious = z2coc(unproject_z(g_depth[coordN - coordDelta], g_proj_constants), 100, .5, g_ddofVals.x);

		float betaPrevious = beta(cocPrevious, ITERATIONS);
		float betaCurrent = beta(cocCurrent, ITERATIONS);
		float betaNext = beta(cocNext, ITERATIONS);

		float betaBackward = min(betaPrevious, betaCurrent * ((i == 0) ? 0 : 1));
		float betaForward = min(betaCurrent, betaNext * ((i == numItems - 1) ? 0 : 1));
		
		float aCur = -1 * betaBackward;
		float bCur = 1 + betaBackward + betaForward;
		float cCur = -1 * betaForward;

		float m = aCur / bPrev;

		float3 dCur = g_color[coordN];
		float b = bCur - m * cPrev;
		g_bc[coordN] = float3(0, b, cCur);
		float3 d = (dCur - m * dPrev);
		
		g_d[coordN] = d;
		bPrev = b;
		cPrev = cCur;
		dPrev = d;
	}
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