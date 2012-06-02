#include "shader.h"

SamplerState g_linearSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

Texture2D <float4> g_albedo;

cbuffer ObjectCB
{
	float4x4 g_wvp;
	float4x4 g_wv;
	uint4 g_misc;
};
cbuffer ObjectAnimationCB
{
	float4x4 gJointTransforms[MAX_BONES];
};
struct VS_IN
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 uv : UV;
	uint4 joints : JOINTS;
	float4 weights : WEIGHTS;
};
struct PS_IN
{	
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : UV;
	float3 posVS : VIEWSPACE_POSITION;
};
struct PS_OUT
{
	float4 normal : SV_TARGET0;
	float3 albedo : SV_TARGET1;
	float4 debug : SV_TARGET2;
	float depth : SV_TARGET3;
};

PS_IN VS(VS_IN IN)
{
	PS_IN OUT;
	float4 skinnedPos = 0;
	float4 skinnedNormal = 0;
	for(int i = 0; i < 4; i++)
	{
		uint bone = IN.joints[i];
		float4x4 t = gJointTransforms[bone];
		skinnedPos += mul(float4(IN.position, 1), t) * IN.weights[i];
		skinnedNormal += mul(float4(IN.normal, 0), t) * IN.weights[i];
	}
	skinnedNormal = float4(normalize(skinnedNormal.xyz), 0);
	OUT.uv = IN.uv;		
	OUT.position = mul(float4(skinnedPos.xyz, 1), g_wvp);
	OUT.normal = mul(skinnedNormal, g_wv).xyz;
	OUT.posVS = mul(float4(skinnedPos.xyz, 1), g_wv).xyz;
	
    return OUT;
}
PS_OUT PS( PS_IN IN)
{
	PS_OUT OUT;
	OUT.depth = IN.position.w / Z_FAR;

	float4 albedo = pow(abs(g_albedo.Sample(g_linearSampler, IN.uv)), 2.2);
	OUT.normal = float4(normalize(IN.normal), 1);
	OUT.albedo = albedo.xyz;
	return OUT;
}
technique10 Geometry
{
	pass p0
	{
		SetVertexShader(CompileShader(vs_4_0, VS()));
		SetPixelShader(CompileShader(ps_4_0, PS()));
	}
}