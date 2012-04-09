#include "shader.h"

Texture2DMS<float4, MSAA_COUNT> g_source : register(t0);

float4 vs(float3 position : POSITION) : SV_POSITION
{	
	return float4(position, 1);
}
float4 ps( float4 position : SV_POSITION) : SV_Target
{			
	float4 total = 0;
	for(int i = 0; i < MSAA_COUNT; i++)
	{
		total += g_source.Load(int2(position.xy), i);
	}
	return total / MSAA_COUNT;
}