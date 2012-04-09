#include "shader.h"
#if MSAA_COUNT > 1
Texture2DMS<float3, MSAA_COUNT> g_source : register(t0);
#else
Texture2D<float3> g_source : register(t0);
#endif

cbuffer LumHighPassCb : register(b0)
{
	float4 g_min_lum;
};
float4 vs(float3 position : POSITION) : SV_POSITION
{	
	return float4(position, 1);
}
float4 ps( float4 position : SV_POSITION) : SV_Target
{			
	//float3 source = g_source.Load(int3(position.xy, 0));
	int passed = 0;
	float3 total = 0;
	for(int i = 0; i < MSAA_COUNT; i++)
	{
		//float3 source = g_source.Load(int3(position.xy), i);
		
		float3 source = g_source.Load(int2(position.xy), i);
		float y = 0.2126 * source.r + 0.7152 * source.g + 0.0722 * source.b;
		if(y>g_min_lum.x)
		{
			return float4(source, 1);
			passed++;
			total += source;
		}
	}
	//float3 source = g_source.Load(int2(position.xy), 0);
	
	//return (y > g_min_lum.x) * float4(source, 1);
	if(passed > 0)	return float4(total / (float)passed, 1);
	else return 0;
}