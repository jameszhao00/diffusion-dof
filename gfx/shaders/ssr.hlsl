#include "shader.h"
/*
Texture2DMS<float4, MSAA_COUNT> g_normal : register(t[0]);
Texture2DMS<float3, MSAA_COUNT> g_albedo : register(t[1]);
Texture2DMS<float, MSAA_COUNT> g_depth : register(t[2]);
Texture2DMS<float4, MSAA_COUNT> g_debug: register(t[3]);
*/
#if MSAA_COUNT > 1
Texture2DMS<float4, MSAA_COUNT> g_normal : register(t[0]);
Texture2DMS<float3, MSAA_COUNT> g_albedo : register(t[1]);
Texture2DMS<float, MSAA_COUNT> g_depth : register(t[2]);
Texture2DMS<float4, MSAA_COUNT> g_debug: register(t[3]);
#else
Texture2D<float4> g_normal : register(t[0]);
Texture2D<float3> g_albedo : register(t[1]);
Texture2D<float> g_depth : register(t[2]);
Texture2D<float4> g_debug: register(t[3]);
#endif

cbuffer FSQuadCB
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
VS2PS vs(float3 position : POSITION)
{	
	VS2PS OUT;
	OUT.position = float4(position.xy, 0.5, 1);
	float4 vs_ray = mul(float4(position.xy, 1, 1), g_inv_p);
	OUT.viewspace_ray = float3(vs_ray.xy / vs_ray.z, 1);
	return OUT;
}
float4 ps( VS2PS IN) : SV_Target
{		
	uint2 vp_size;
	uint vp_msaa_count;
#if MSAA_COUNT > 1
	g_normal.GetDimensions(vp_size.x, vp_size.y, vp_msaa_count);
#else
	g_normal.GetDimensions(vp_size.x, vp_size.y);
#endif
	
	//float3 pix_coord = float3(IN.position.xy, 0);
	float2 pix_coord = float2(IN.position.xy);
	bool DEBUG_SWITCH = false;
	if(DEBUG_SWITCH)
	{
		pix_coord.x = 417.5;
		pix_coord.y = 242.5;
		IN.viewspace_ray = float3( 0.024, 0.079, 1.000 );

	}
	else if( g_debug.Load(pix_coord, 0).x == 0)
	{
		return g_albedo.Load(pix_coord, 0).xyzz;
	}
	
	float ndc_depth = g_depth.Load(pix_coord, 0);
	float3 vs_pos = get_vs_pos(IN.viewspace_ray, ndc_depth, g_proj_constants.xy);
	//if(ndc_depth == DEPTH_MAX) return 0;
	//be careful about the XYZ when normalizing!!!
	float3 vs_dir = normalize(IN.viewspace_ray.xyz);
	//sample view space normal vector
	float3 s_vs_n = normalize(g_normal.Load(pix_coord, 0).xyz);

	

	//sample view space reflection vector

	float3 s_vs_r = reflect(vs_dir, s_vs_n);

	//float blend_factor = dot(vs_dir, s_vs_r);
	//if(blend_factor < -0.5) return 0.8;
	float blend_factor = 1;

	float3 s_vs_search_pos = vs_pos + 2 * s_vs_r;

	for(float d = 1; d < 50; d++)
	{
		float2 c = vs_to_vp(vs_pos + d * s_vs_r, vp_size, g_proj);
			//return c.x / 800;
		//if(length(c - IN.position.xy) < 5) return RED;
	}
	//if(DEBUG_SWITCH) return g_albedo.Load(IN.position, 0).xyzz;
	if(DEBUG_SWITCH) s_vs_search_pos = s_vs_search_pos + 5 * s_vs_r;
	int counter;
	float increment = 0.3;
	for(int i = 0; i < 1000; i++)
	{
		counter++;
		s_vs_search_pos += increment * s_vs_r;
		//increment += 0.1;
		float2 s_vp_search_pos = vs_to_vp(s_vs_search_pos, vp_size, g_proj);

		if(
			(s_vp_search_pos.x < 0) ||
			(s_vp_search_pos.x > (int)vp_size.x) ||
			(s_vp_search_pos.y < 0) ||
			(s_vp_search_pos.y > (int)vp_size.y))
		{
			break;
		}

		float s_vs_search_expected_z = s_vs_search_pos.z;

		s_vp_search_pos = floor(s_vp_search_pos) + float2(0.5, 0.5);
		//float s_ndc_search_depth = g_depth.Load(float3(s_vp_search_pos, 0));
		float s_ndc_search_depth = g_depth.Load(float2(s_vp_search_pos), 0);
		float s_vs_search_actual_z = unproject_z(s_ndc_search_depth, g_proj_constants.xy);
		float error = abs(s_vs_search_actual_z - s_vs_search_expected_z);

		if(DEBUG_SWITCH)
		{
			if(length(s_vp_search_pos - IN.position.xy) < 3)
			{
				return RED;
			}
			if(error < .5) break;
		}
		else
		{
			if(error < 2)
			{
				//return 50 * error * RED; 
				//return blend_factor * .6 * g_albedo.Load(float3(s_vp_search_pos, 0)).xyzz + .4;
				return blend_factor * .6 * g_albedo.Load(float2(s_vp_search_pos), 0).xyzz + .4;
				break;
			}
		}		
	}
	//if(DEBUG_SWITCH) return g_albedo.Load(int3(IN.position.xy, 0), 0).xyzz;
	//return RED;
	//return counter/2000.0 * RED;
	//return RED;
	return .8;
	//return g_albedo.Load(pix_coord, 0).xyzz;;

}