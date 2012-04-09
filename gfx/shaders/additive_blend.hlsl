#include "shader.h"

Texture2D<float4> g_source_a : register(t0);
Texture2D<float4> g_source_b : register(t1);

float4 vs(float3 position : POSITION) : SV_POSITION
{	
	return float4(position, 1);
}
float4 ps( float4 position : SV_POSITION) : SV_Target
{			
	float4 a = g_source_a.Load(int3(position.xy, 0), 0);
	float4 b = g_source_b.Load(int3(position.xy, 0), 0);
	return float4(a.xyz + b.xyz, 1);
}