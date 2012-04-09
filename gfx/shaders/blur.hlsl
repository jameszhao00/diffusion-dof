#include "shader.h"
SamplerState g_blur_sampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};
//#if MSAA_COUNT > 1
//Texture2D<float4, MSAA_COUNT> g_source : register(t0);
//#else
Texture2D<float4> g_source : register(t0);
//#endif

//make sure sampler is CLAMP!!

cbuffer BlurCB : register(b0)
{
	/*
	weight layout
	pixels:		+	+   +   +   +   +   +   +   +
	weights:	 (2)     (1)   (0)   (1)     (2)
	*/

	float4 g_offsets[4]; //for bilinear stuff - offsets[0] is the 1st pixel AFTER [i, j]
	//offsets are in UV coordinates
	float4 g_norms[4]; //normalization per weight
	
	int4 g_offsets_count; //# of samples apart from img[i, j]
};
struct BlurVS2PS
{
	float4 position : SV_POSITION;
	float2 uv : UV;
};
BlurVS2PS vs(float3 position : POSITION)
{	
	BlurVS2PS OUT;
	OUT.position = float4(position, 1);
	OUT.uv = position.xy / 2 + .5;
	return OUT;
}
float4 blur(float2 uv, float2 offset_basis)
{
	
	/* always 0: * g_offsets[0] */
	float4 result = g_norms[0][0] * g_source.Sample(g_blur_sampler, uv);
	for(uint i = 1; i < (uint)g_offsets_count.x; i++)
	{
		uint major_index = i/4;
		uint minor_index = i%4;
		//we offset by +1 because g_*[0] is the center pixel's stuff
		float2 offset = g_offsets[major_index][minor_index] * offset_basis; //offsets's [0] is not [i, j], but the sample after it
		float norm = g_norms[major_index][minor_index];
		result += norm * g_source.Sample(g_blur_sampler, uv + offset);
		result += norm * g_source.Sample(g_blur_sampler, uv - offset);
	}
	return result;
}
float4 ps_blur_x( BlurVS2PS IN ) : SV_Target
{	
	//return g_source.Sample(g_blur_sampler, IN.uv);//blur(IN.uv, float2(0, 1));
	return blur(IN.uv, float2(1, 0));
}
float4 ps_blur_y( BlurVS2PS IN ) : SV_Target
{		
	//return g_source.Sample(g_blur_sampler, IN.uv);//blur(IN.uv, float2(0, 1));
	
	return blur(IN.uv, float2(0, 1));
}