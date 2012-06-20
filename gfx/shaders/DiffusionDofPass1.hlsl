#include "shader.h"
Texture2D<float> g_depth;
Texture2D<float3> g_color;

RWTexture2D<float3> g_bc;
RWTexture2D<float3> g_d;

cbuffer DDofCB
{
	float4 g_coc;
};
cbuffer FSQuadCB : register(b1)
{	
	float4x4 g_inv_p;
	float4 g_proj_constants;
	float4 g_debug_vars;
	float4x4 g_proj;
};

float beta(float coc)
{
	return clamp(coc * coc * .5, 0, 1800);
}
float z2coc(float sampleZ)
{
	//from http://http.developer.nvidia.com/GPUGems/gpugems_ch23.html
	float aperture = 100;
	float focallength = .5;
	float planeinfocus = g_coc.x;


	return abs(aperture * (focallength * (sampleZ - planeinfocus)) /
          (sampleZ * (planeinfocus - focallength)));
			
}

[numthreads(32, 1, 1)]
void csPass1H( uint3 DTid : SV_DispatchThreadID )
{
	float2 size;
	//work horizontally.. very inefficient
	g_depth.GetDimensions(size.x, size.y);
	//depends on primary direction
	float primarySize = size.y;
	float secondarySize = size.x;
	if(DTid.x > (primarySize - 1)) return; //we're out of bounds
	float focalDistance = g_coc.x;
	int2 coord0 = int2(0, DTid.x);
	
	float betaPrevious = 0;
	float cocCurrent = z2coc(unproject_z(g_depth[coord0], g_proj_constants));
	float betaCurrent = beta(cocCurrent);
	
	float cocNext = z2coc(unproject_z(g_depth[coord0 + int2(1, 0)], g_proj_constants));
	float betaNext = beta(cocNext);
	float betaBackward = 1;
	float betaForward = min(betaCurrent, betaNext);
	float bPrev = 1 + betaBackward + betaForward;;
	float cPrev = -betaForward;
	//depends on primary direction
	float3 dPrev = g_color[coord0];
	g_bc[coord0] = float3(0, 3, -1);
	g_d[coord0] = dPrev;

	for(int i = 1; i < secondarySize; i++)
	{
		int2 coordN = int2(i, DTid.x);
		betaPrevious = betaCurrent;
		betaCurrent = betaNext;
		//dependent
		float cocNext = z2coc(unproject_z(g_depth[coordN + int2(1, 0)], g_proj_constants));
		betaNext = beta(cocNext);
		float betaBackward = min(betaPrevious, betaCurrent);
		float betaForward = min(betaCurrent, betaNext * ((i == secondarySize - 1) ? 0 : 1));
		//depends on primary direction
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
void csPass1V( uint3 DTid : SV_DispatchThreadID )
{
	float2 size;
	//work horizontally.. very inefficient
	g_depth.GetDimensions(size.x, size.y);
	//depends on primary direction
	float primarySize = size.x;
	float secondarySize = size.y;
	if(DTid.x > (primarySize - 1)) return; //we're out of bounds
	float focalDistance = g_coc.x;
	int2 coord0 = int2(DTid.x, 0);
	
	float betaPrevious = 1;
	float cocCurrent = z2coc(unproject_z(g_depth[coord0], g_proj_constants));
	float betaCurrent = beta(cocCurrent);
	
	float cocNext = z2coc(unproject_z(g_depth[coord0 + int2(0, 1)], g_proj_constants));
	float betaNext = beta(cocNext);
	float betaBackward = 1;
	float betaForward = min(betaCurrent, betaNext);
	float bPrev = 1 + betaBackward + betaForward;;
	float cPrev = -betaForward;
	//depends on primary direction
	float3 dPrev = g_color[coord0];
	g_bc[coord0] = float3(0, 3, -1);
	g_d[coord0] = dPrev;


	for(int i = 1; i < secondarySize; i++)
	{
		int2 coordN = int2(DTid.x, i);
		betaPrevious = betaCurrent;
		betaCurrent = betaNext;
		//dependent
		float cocNext = z2coc(unproject_z(g_depth[coordN + int2(0, 1)], g_proj_constants));
		betaNext = beta(cocNext);
		float betaBackward = min(betaPrevious, betaCurrent);
		float betaForward = min(betaCurrent, betaNext * ((i == secondarySize - 1) ? 0 : 1));
		//depends on primary direction
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