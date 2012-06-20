#include "shader.h"

Texture2D<float3> g_color;
Texture2D<float> g_depth;

cbuffer DofCB
{
	float4 g_cocPower;
};

struct VS2GS
{	
	float2 ndcRadius : RADIUS; //half of ndc width
	float3 color : COLOR;
	float4 ndcPos : SV_POSITION; //pre-transformed to ndc
};
struct GS2PS
{
	float3 color : COLOR;
	float2 uv : UV;
	float4 ndcPos : SV_POSITION;
};
VS2GS vs(uint vertexId : SV_VertexID)
{
	VS2GS OUT;
	float2 viewportSize;
	g_depth.GetDimensions(viewportSize.x, viewportSize.y);
	int2 coord = int2(vertexId % viewportSize.x, viewportSize.y - vertexId / viewportSize.x - 1);
	//float g_focalLength = 10;
	//float zNdc = g_depth.Load(int3(coord, 0));
	//float zVs = unproject_z(zNdc, g_proj_constants);
	float bokehRadius = g_cocPower.x;
	float bokehArea = 3.1415 * bokehRadius * bokehRadius;
	OUT.color = g_color.Load(int3(coord, 0)) / bokehArea;
	OUT.ndcRadius = bokehRadius / viewportSize;
	OUT.ndcPos = float4(float2(1, -1) * (coord / viewportSize - 0.5) * 2, 0, 1);
	return OUT;
}
[maxvertexcount(4)]
void gs(point VS2GS IN[1], inout TriangleStream<GS2PS> OUT)
{
	if(dot(IN[0].color, IN[0].color) == 0) return;

	GS2PS upperLeft; 
	upperLeft.ndcPos = IN[0].ndcPos + float4(float2(-1, 1) * IN[0].ndcRadius, 0, 0);
	upperLeft.color = IN[0].color;
	upperLeft.uv = float2(-1, -1);
	OUT.Append(upperLeft);
	
	GS2PS upperRight; 
	upperRight.ndcPos = IN[0].ndcPos + float4(float2(1, 1) * IN[0].ndcRadius, 0, 0);
	upperRight.color = IN[0].color;
	upperRight.uv = float2(1, -1);
	OUT.Append(upperRight);
	
	GS2PS lowerLeft; 
	lowerLeft.ndcPos = IN[0].ndcPos + float4(float2(-1, -1) * IN[0].ndcRadius, 0, 0);
	lowerLeft.color = IN[0].color;
	lowerLeft.uv = float2(-1, 1);
	OUT.Append(lowerLeft);
	
	GS2PS lowerRight; 
	lowerRight.ndcPos = IN[0].ndcPos + float4(float2(1, -1) * IN[0].ndcRadius, 0, 0);
	lowerRight.color = IN[0].color;
	lowerRight.uv = float2(1, 1);
	OUT.Append(lowerRight);
}
float4 ps( GS2PS IN) : SV_TARGET
{
	//circle
	if(dot(IN.uv, IN.uv) < 1) return float4(IN.color, 1);
	return 0;
}