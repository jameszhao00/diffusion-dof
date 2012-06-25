
#include "diffusiondof.h"
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
	uint4 r = ddofPack(1, 2, 3, float3(.35,.65, .77));
		float a, b, c;
	float3 d;
	ddofUnpack(r, a, b, c, d);
	
	//uint ns = 0, s = 0;
	//g_debugIn.GetDimensions(ns, s);
	float2 size = 0;
	//return abs(d.x - .35) < 0.01 && abs(d.y - .65) < 0.01 && abs(d.z - .77) < 0.02;
	//return abs(d.x - .35) > 0.0005;
	//return  abs(a - 1) < 0.01 && abs(b - 2) < 0.01 && abs(c - 3) < 0.01;
	g_source.GetDimensions(size.x, size.y);	
	//return d.xyzz;
	float3 color = g_debugIn[floor(position.y) * size.x + floor(position.x)].color.xyz;
	return float4(color, 1);

}