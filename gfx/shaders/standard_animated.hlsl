#include "shader.h"

SamplerState g_albedo_sampler;

Texture2D <float4> g_albedo;

cbuffer ObjectCB
{
	float4x4 g_wvp;
	float4x4 g_wv;
};
cbuffer ObjectAnimationCB
{
	float4x4 g_bone_transforms[MAX_BONES];
};
struct App2VS
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 uv : UV;
	uint4 bones : BONES;
	float4 bone_weights : BONE_WEIGHTS;
};
struct VS2PS
{	
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : UV;
	//for debug only (i think)
	float3 vs_pos : VIEWSPACE_POSITION;
};
struct PS_OUT
{
	float4 normal : SV_TARGET0;
	float4 albedo : SV_TARGET1;
	float3 debugRef : SV_TARGET2;
};
VS2PS vs(App2VS IN)
{
	VS2PS OUT;
	
	float4 skinned_pos = 0;
	float4 skinned_normal = 0;
	if(IN.bone_weights[0] == 0)
	{	
		skinned_pos = float4(IN.position, 1);
		skinned_normal = float4(IN.normal, 0);
	}
	for(int i = 0; i < 4; i++)
	{
		uint bone = IN.bones[i];
		float4x4 t = g_bone_transforms[bone];
		skinned_pos += mul(float4(IN.position, 1), t) * IN.bone_weights[i];
		skinned_normal += mul(float4(IN.normal, 0), t) * IN.bone_weights[i];
	}
	skinned_normal = float4(normalize(skinned_normal.xyz), 0);
	OUT.uv = IN.uv;	
	{
		OUT.position = mul(float4(skinned_pos.xyz, 1), g_wvp);
		OUT.normal = mul(skinned_normal, g_wv).xyz;
		OUT.vs_pos = mul(float4(skinned_pos.xyz, 1), g_wv).xyz;
	}
    return OUT;
}
PS_OUT ps( VS2PS IN)
{
	PS_OUT OUT;
	//hack to fix gamma.... i use d3dx to load textures...
	float4 albedo = pow(abs(g_albedo.Sample(g_albedo_sampler, IN.uv)), 2.2);
	OUT.normal = float4(normalize(IN.normal), 1);
	OUT.albedo = float4(albedo.xyz, 1);
	OUT.debugRef = IN.vs_pos;
	return OUT;
}