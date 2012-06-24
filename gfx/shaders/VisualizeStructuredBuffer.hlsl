#include "shader.h"
struct DDofOutput
{
	float3 color;
};
Texture2D<float3> g_source : register(t0);
StructuredBuffer<DDofOutput> g_debugIn : register(t1);

float4 vs(float3 position : POSITION) : SV_POSITION
{	
	return float4(position, 1);
}
float4 ps( float4 position : SV_POSITION) : SV_Target
{		
	float2 size = 0;
	g_source.GetDimensions(size.x, size.y);	

	return g_debugIn[position.y * size.x + position.x].color.xyzz;

}