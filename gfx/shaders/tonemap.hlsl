#include "shader.h"
#if MSAA_COUNT > 1
Texture2DMS<float3, MSAA_COUNT> g_source : register(t0);
#else
Texture2D<float3> g_source : register(t0);
#endif
//Texture2D<float> g_avg_lum : register(t1);
/*
cbuffer TonemapCB : register(b0)
{
	float4 g_constants;
};
*/
float4 vs(float3 position : POSITION) : SV_POSITION
{	
	return float4(position, 1);
}
float4 ps( float4 position : SV_POSITION) : SV_Target
{			
	float3 tonemapped = 0;
	float3 pix_coord = float3(position.xy, 0);
	//for(int i = 0; i < MSAA_COUNT; i++)
	int i = 1;
	{
		float3 linear_color = g_source.Load(pix_coord, i);

		// from Naughty Dog's Filmic curves section
		//float3 x = max(0, linear_color-0.004);
		//float3 GammaColor = (x*(6.2*x+0.5))/(x*(6.2*x+1.7)+0.06);
		//tonemapped += GammaColor;

		tonemapped += linear_color / (1 + linear_color);
	}
	return float4(tonemapped, 1);
}