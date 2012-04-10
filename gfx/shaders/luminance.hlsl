#include "shader.h"
#if MSAA_COUNT > 1
Texture2DMS<float3, MSAA_COUNT> g_source : register(t0);
#else
Texture2D<float3> g_source : register(t0);
#endif

float4 vs(float3 position : POSITION) : SV_POSITION
{	
	return float4(position, 1);
}
float4 ps( float4 position : SV_POSITION) : SV_Target
{			
	float total = 0;
	for(int i = 0; i < MSAA_COUNT; i++)
	{
		float3 source = g_source.Load(int2(position.xy), i);
		total += luminance(source);
	}
	return total / MSAA_COUNT;
}