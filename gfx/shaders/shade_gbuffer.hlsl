#include "shader.h"


#if MSAA_COUNT > 1
Texture2DMS<float4, MSAA_COUNT> g_normal : register(t0);
Texture2DMS<float3, MSAA_COUNT> g_albedo : register(t1);
Texture2DMS<float, MSAA_COUNT> g_depth : register(t2);
#else
Texture2D<float4> g_normal : register(t0);
Texture2D<float3> g_albedo : register(t1);
Texture2D<float> g_depth : register(t2);
Texture2D<float4> g_debugRef : register(t3);
#endif


cbuffer ShadeGBufferDebugCB : register(b0)
{
	int g_render_mode; 
	int g_usefresnel;
	int2 padding;
	float4x4 g_view;
	float4 g_light_dir_ws;
};

cbuffer FSQuadCB : register(b1)
{	
	float4x4 g_inv_p;
	float4 g_proj_constants;
	float4 g_debug_vars;
	float4x4 g_proj;
};
struct VS2PS
{
	float4 position : SV_POSITION;
	float3 viewspace_ray : RAY0;
};
struct PS2GPU
{
	float4 color : SV_TARGET0;
};
VS2PS vs(float3 position : POSITION)
{	
	VS2PS OUT;
    OUT.position = float4(position.xy, 0.5, 1);
	float4 vs_ray = mul(float4(position.xy, 1, 1), g_inv_p);
	OUT.viewspace_ray = float3(vs_ray.xy / vs_ray.z, 1);
	return OUT;
}

float4 ps( VS2PS IN ) : SV_TARGET
{	
	int3 coord = int3(IN.position.xy, 0);
	float4 vs_light_pos = mul(float4(g_light_dir_ws.xyz, 1), g_view);
	float zNdc = g_depth.Load(coord, 0).x;
	float vs_z = unproject_z(zNdc, g_proj_constants);
	if(zNdc == 0) return float4(.2, .3, .5, 1);
	float3 vs_pos = IN.viewspace_ray * vs_z;
	
	float3 vs_light_dir = normalize(vs_light_pos.xyz - vs_pos);
	float3 vs_dir = normalize(vs_pos);
	float3 vs_h = normalize(vs_light_dir + (-vs_dir));
	float3 c_light = 4;
	float3 color = 0;
	int sample_i = 0;
	{		
		float3 normal = normalize(g_normal.Load(coord, sample_i).xyz);
		float3 albedo = g_albedo.Load(coord, sample_i);
		if(vs_z != Z_FAR)
		{	
			if(1)
			{
				float glossiness = 90;
				float spec_normalization = (glossiness + 2) / (2 * 3.1415);
				float brdf_normalization = 3.1415 / 4;
				float spec = pow(max(dot(normal, vs_h), 0), glossiness);
				float d = spec_normalization * spec;
				float3 f0 = float3(0.2, 0.2, 0.2); //schlick f0
				float3 f = f0 + (1 - f0)*pow(1 - dot(vs_light_dir, vs_h), 5);
				float ndotl = max(dot(normal, vs_light_dir), 0);
				//does fresnel work?
				float3 brdf = brdf_normalization * d * (f *( g_usefresnel ? f : 1));
				color += (albedo + brdf) * ndotl * c_light;
			}
			else
			{	
				color += albedo;
			}
		}
		else
		{
			color = float4(.2, .3, .5, 1);//RED.xyz;
		}
		
	}
	color /= MSAA_COUNT;

	return color.xyzz;
}