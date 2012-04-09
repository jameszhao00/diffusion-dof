#define MAX_BONES 64

SamplerState g_albedo_sampler;

Texture2D <float4> g_albedo;

cbuffer ObjectCB
{
	float4x4 g_wvp;
	float4x4 g_wv;
	uint4 misc;
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
struct VS2GS
{	
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : UV;
	//for debug only (i think)
	float3 vs_pos : VIEWSPACE_POSITION;
	float3 ndc_pos : NDC_POSITION;
};
struct GS2PS
{	
	centroid float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : UV;
	float3 vs_pos : VIEWSPACE_POSITION;

	float3 ndc_tri_verts[3] : TRIANGLE;
	uint tri_id : TRIANGLE_ID;
	nointerpolation float3 debug1 : DEBUG1;
};

struct PS_OUT
{
	float4 normal : SV_TARGET0;
	float3 albedo : SV_TARGET1;
	float4 debug : SV_TARGET2;

	float4 ndc_tri_vert0 : SV_TARGET3;
	float4 ndc_tri_vert1 : SV_TARGET4;
	float4 ndc_tri_vert2 : SV_TARGET5;
};
VS2GS vs(App2VS IN)
{
	VS2GS OUT;
	float4 skinned_pos = 0;
	float4 skinned_normal = 0;
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
		OUT.vs_pos = mul(float4(skinned_pos.xyz, 1), g_wv);
		OUT.ndc_pos = OUT.position.xyz / OUT.position.w;
	}
	/*
	{		
		OUT.position = mul(float4(IN.position.xyz, 1), g_wvp);
		OUT.normal = mul(float4(IN.normal, 1), g_wv).xyz;
		OUT.vs_pos = mul(float4(IN.position.xyz, 1), g_wv);
		OUT.ndc_pos = OUT.position.xyz / OUT.position.w;
	}
	*/
    return OUT;
}
[maxvertexcount(3)]
void gs( triangle VS2GS IN[3], inout TriangleStream<GS2PS> OUT, uint primitive_id : SV_PrimitiveID  )
{	
	for(int i = 0; i < 3; i++)
	{
		GS2PS out_tri = (GS2PS)0;
		
		out_tri.position = IN[i].position;
		out_tri.normal = IN[i].normal;
		out_tri.uv = IN[i].uv;
		out_tri.vs_pos = IN[i].vs_pos;
		out_tri.ndc_tri_verts[0] = IN[0].ndc_pos;
		out_tri.ndc_tri_verts[1] = IN[1].ndc_pos;
		out_tri.ndc_tri_verts[2] = IN[2].ndc_pos;
		out_tri.tri_id = primitive_id;
		out_tri.debug1 = float3(1, 2, 3);
		OUT.Append(out_tri);
	}
	OUT.RestartStrip();
}

PS_OUT ps( GS2PS IN)//, uint sample_index : SV_SampleIndex )
{
	PS_OUT OUT;
	if(misc[0] == 1)
	{
		OUT.normal = 1;
		OUT.albedo = 0;
		OUT.debug = 0;
		return OUT;
	}
	
	//hack to fix gamma.... i use d3dx to load textures...
	float4 albedo = pow(g_albedo.Sample(g_albedo_sampler, IN.uv), 2.2);
	//OUT.normal = float4(normalize(IN.normal), 1);
	OUT.normal = float4(IN.normal, 1);
	OUT.albedo = albedo.xyz;
	OUT.debug = length(frac(IN.position.xy) - 0.5) > 0;
	float2 dz = float2(ddx(IN.vs_pos.z), ddy(IN.vs_pos.z));
	OUT.debug.x = length(dz * dz) > 0.05;
	//OUT.debug.w = (sample_index + 1) / 4.0;
	//OUT.debug.x = IN.debug1.x / 4;
	//OUT.debug.x = g_base_primitive_id;
	//OUT.debug = length(IN.vs_pos.xyz);
	//OUT.debug = IN.vs_pos.z;
	OUT.ndc_tri_vert0 = IN.ndc_tri_verts[0].xyzz * 0.8;
	OUT.ndc_tri_vert1 = IN.ndc_tri_verts[1].xyzz * 0.8;
	OUT.ndc_tri_vert2 = IN.ndc_tri_verts[2].xyzz * 0.8;
	
	//format is snorm... so mul by scale
	OUT.ndc_tri_vert0.w = IN.tri_id * 0.000001;
	OUT.ndc_tri_vert1.w = IN.tri_id * 0.000001;
	OUT.ndc_tri_vert2.w = IN.tri_id * 0.000001;
	return OUT;
	/*
	albedo.w = 1;
	float3 n = normalize(IN.normal);
    return saturate(saturate(ndotl)) * albedo;    // Yellow, with Alpha = 1
	*/
}