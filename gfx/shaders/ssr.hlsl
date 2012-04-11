#include "shader.h"
SamplerState g_linear : register(s[0]);
SamplerState g_aniso : register(s[1]);

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
	float4 g_debug_vars; //g_debug_vars[0] should have 0.5 * cot(fov)
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



float4 ps(VS2PS IN) : SV_TARGET
{
	uint2 vp_size;
	g_normal.GetDimensions(vp_size.x, vp_size.y);
	float3 pix_coord = float3(IN.position.xy, 0);
	
	float ndc_depth = g_depth.Load(pix_coord, 0);
	float3 pos_vs = get_vs_pos(IN.viewspace_ray, ndc_depth, g_proj_constants.xy);
	//if(ndc_depth == DEPTH_MAX) return 0;
	//be careful about the XYZ when normalizing!!!
	float3 dir_vs = normalize(IN.viewspace_ray.xyz);
	//sample view space normal vector
	float3 n_vs = normalize(g_normal.Load(pix_coord, 0).xyz);

	float3 r_vs = reflect(dir_vs, n_vs);
	//r_vs = normalize(float3(1, 2, 1)); //TEST!!
	//pos_vs = (float3(2, 2, 20)); //TEST!!
	float2 r_ss = normalize(vs_to_vp(pos_vs + r_vs, vp_size, g_proj) - pix_coord.xy);

	//s_* = sample_

	//buffer by 1 r_ss
	float s_y_ss = pix_coord.y;
	float s_x_ss = pix_coord.x;
	float half_cot_fov = g_debug_vars[0];

	
	float last_test_z_vs_err = 1000; //actual - expected ... z error in VS 
	float3 last_test_t;
	float increment = 10;
	for(int i = 0; i < 18; i++)
	{
		//we can't increment this directly...
		//we have to scale it so that an increment will cross approximately 1 pixel
		s_y_ss += increment * r_ss.y;
		s_x_ss += increment * r_ss.x; 
		increment += 3;
		if(
			(s_y_ss < 0) ||
			(s_y_ss > (int)vp_size.x) ||
			(s_y_ss < 0) ||
			(s_y_ss > (int)vp_size.y))
		{
			break;
		}

		//s_y_ss = 5; //TEST!!
		//calculate the desired t. look at math.nb
		float t = 
			(-half_cot_fov * vp_size.y * pos_vs.y + (-2 * s_y_ss + vp_size.y) * pos_vs.z) / 
			(2 * s_y_ss * r_vs.z - r_vs.z * vp_size.y + r_vs.y * half_cot_fov * vp_size.y);
		
		//return abs(t); //TEST

		//return t;
		float s_desired_z_ndc = 
			(g_proj_constants[1] + g_proj_constants[0]*(r_vs.z * t + pos_vs.z)) / 
			(r_vs.z * t + pos_vs.z);

		//return s_desired_z_ndc;
		float2 s_pos_uv = vp_to_uv(vp_size, float2(s_x_ss, s_y_ss));
		float s_actual_z_ndc = g_depth.SampleGrad (g_linear, s_pos_uv, 0, 0);

		float s_actual_z_vs = unproject_z(s_actual_z_ndc, g_proj_constants.xy);
		float s_desired_z_vs = unproject_z(s_desired_z_ndc, g_proj_constants.xy);
		float s_z_error_vs = s_actual_z_vs - s_desired_z_vs;

		if(
			abs(s_z_error_vs) < 8 && //current error is acceptable
			abs(last_test_z_vs_err < 8) && //last error is acceptable
			(sign(s_z_error_vs) != sign(last_test_z_vs_err)) //we've crossed a boundary
			
			)
		{

			float err_ratio = abs(last_test_z_vs_err) / (abs(s_z_error_vs) + abs(last_test_z_vs_err));
			
			float3 last_test_vs_pos = pos_vs + last_test_t * r_vs;
			float3 current_test_vs_pos = pos_vs + t * r_vs;

			float3 interpolated_sample_pos_vs = lerp(last_test_vs_pos, current_test_vs_pos, err_ratio);				

			float2 interpolated_sample_pos_vp = vs_to_vp(interpolated_sample_pos_vs, vp_size, g_proj);
								
			float2 interpolated_sample_pos_uv = vp_to_uv(vp_size, interpolated_sample_pos_vp);			

			return float4(g_albedo.SampleGrad(g_linear, interpolated_sample_pos_uv, 0, 0), 1);
		}
		
		last_test_t = t;
		last_test_z_vs_err = s_z_error_vs;
	}
	return float4(g_albedo.Load(pix_coord, 0).xyz, 1);

}
float4 ps1( VS2PS IN) : SV_Target
{		

	uint2 vp_size;
	uint vp_msaa_count;
#if MSAA_COUNT > 1
	g_normal.GetDimensions(vp_size.x, vp_size.y, vp_msaa_count);
#else
	g_normal.GetDimensions(vp_size.x, vp_size.y);
#endif
	
	float3 pix_coord = float3(IN.position.xy, 0);
	//float2 pix_coord = float2(IN.position.xy);
	bool DEBUG_SWITCH = false;
	if(DEBUG_SWITCH)
	{
		pix_coord.x = 417.5;
		pix_coord.y = 242.5;
		IN.viewspace_ray = float3( 0.024, 0.079, 1.000 );

	}
	else if(0)// g_debug.Load(pix_coord, 0).x == 0)
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

	float3 s_vs_search_pos = vs_pos + s_vs_r;

	for(float d = 1; d < 50; d++)
	{
		float2 c = vs_to_vp(vs_pos + d * s_vs_r, vp_size, g_proj);
			//return c.x / 800;
		//if(length(c - IN.position.xy) < 5) return RED;
	}
	//if(DEBUG_SWITCH) return g_albedo.Load(IN.position, 0).xyzz;
	if(DEBUG_SWITCH) s_vs_search_pos = s_vs_search_pos + s_vs_r;
	int counter;
	float increment = 1;
	float last_test_z_vs_err = 1000; //actual - expected ... z error in VS 
	float3 last_test_vs_pos = s_vs_search_pos;
	for(int i = 0; i < 25; i++)
	{
		counter++;
		s_vs_search_pos += increment * s_vs_r;
		
		float2 s_vp_search_pos = vs_to_vp(s_vs_search_pos, vp_size, g_proj);

		if(
			(s_vp_search_pos.x < 0) ||
			(s_vp_search_pos.x > (int)vp_size.x) ||
			(s_vp_search_pos.y < 0) ||
			(s_vp_search_pos.y > (int)vp_size.y))
		{
			return RED;
			break;
		}
		float s_vs_search_expected_z = s_vs_search_pos.z;

		float2 uv_search_pos = vp_to_uv(vp_size, s_vp_search_pos);
		
		float s_ndc_search_depth = g_depth.SampleGrad (g_linear, uv_search_pos, 0, 0);
		float s_vs_search_actual_z = unproject_z(s_ndc_search_depth, g_proj_constants.xy);
		float error = s_vs_search_actual_z - s_vs_search_expected_z;
		
		if(DEBUG_SWITCH)
		{
			if(length(s_vp_search_pos - IN.position.xy) < 5)
			{
				return RED;
			}
		}
		if(
			abs(error) < 2 && //current error is acceptable
			abs(last_test_z_vs_err < 2) && //last error is acceptable
			(sign(error) != sign(last_test_z_vs_err)) //we've crossed a boundary
			)
		{
			//interpolate between them
			//using last z error vs current z error
			float err_ratio = abs(last_test_z_vs_err) / (abs(error) + abs(last_test_z_vs_err));
				
			float3 interpolated_sample_pos_vs = lerp(last_test_vs_pos, s_vs_search_pos, err_ratio);				

			float2 interpolated_sample_pos_vp = vs_to_vp(interpolated_sample_pos_vs, vp_size, g_proj);
								
			float2 interpolated_sample_pos_uv = vp_to_uv(vp_size, interpolated_sample_pos_vp);			

			return float4(g_albedo.SampleGrad(g_linear, interpolated_sample_pos_uv, 0, 0), 1);
		}
				
		last_test_vs_pos = s_vs_search_pos;
		last_test_z_vs_err = error;
	}
	if(DEBUG_SWITCH) return g_albedo.Load(int3(IN.position.xy, 0), 0).xyzz;
	//return RED;
	return float4(g_albedo.Load(pix_coord, 0).xyz, 1);

}