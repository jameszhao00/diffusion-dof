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
#define z_error_bounds 25
#define base_increment 80
cbuffer FSQuadCB
{	
	float4x4 g_inv_p;
	float4 g_proj_constants;
	float4 g_debug_vars; //g_debug_vars[0] should have 0.5 * cot(fov)
	//debug vars[1]/[2] have x/y pos of the pixel to show rays for
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

float4 debug(VS2PS IN)
{
	uint2 vp_size;
	g_normal.GetDimensions(vp_size.x, vp_size.y);
	float3 pix_coord = float3(IN.position.xy, 0);

	///////////DEBUG
	pix_coord = float3(g_debug_vars.yz, 0);

	if(length(IN.position.xy  - pix_coord.xy) < 5) return float4(0, 1, 0, 1);

	float4 vs = mul(float4(vp_to_ndc(vp_size, pix_coord.xy, 0), 1), g_inv_p);
	vs = vs / vs.w;

	IN.viewspace_ray = normalize(vs.xyz);
	///////////////////////
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

	//idea: increment in small steps the further away the ray is

	for(int i = 0; i < max(vp_size.x, vp_size.y) / base_increment; i++)
	{
		//we can't increment this directly...
		//we have to scale it so that an increment will cross approximately 1 pixel
		float increment = base_increment;//(base_increment + i) * clamp(abs(last_test_z_vs_err) / 10, .7, 1.5);
		
		s_y_ss += increment * r_ss.y;
		s_x_ss += increment * r_ss.x; 
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

		//we can't bilinearly sample Z
		//float s_actual_z_ndc = g_depth.SampleGrad (g_linear, s_pos_uv, 0, 0);
		float s_actual_z_ndc = g_depth.Load(int3(s_x_ss, s_y_ss, 0));
		//if we Load ndc Z, we can't interpolate based on this

		float s_actual_z_vs = unproject_z(s_actual_z_ndc, g_proj_constants.xy);
		float s_desired_z_vs = unproject_z(s_desired_z_ndc, g_proj_constants.xy);
		float s_z_error_vs = s_actual_z_vs - s_desired_z_vs;
		
		//(sign(s_z_error_vs) != sign(last_test_z_vs_err))
		//the above doesnt work because it also succeeds when the ray poke out of something

		if(
			abs(s_z_error_vs) < z_error_bounds && //current error is acceptable
			abs(last_test_z_vs_err < z_error_bounds) && //last error is acceptable
			(last_test_z_vs_err > 0) &&
			(s_z_error_vs < 0)
			
			)
		{
			float err_ratio = abs(last_test_z_vs_err) / (abs(s_z_error_vs) + abs(last_test_z_vs_err));
			
			float3 last_test_vs_pos = pos_vs + last_test_t * r_vs;
			float3 current_test_vs_pos = pos_vs + t * r_vs;

			float3 interpolated_sample_pos_vs = lerp(last_test_vs_pos, current_test_vs_pos, err_ratio);				

			float2 interpolated_sample_pos_vp = vs_to_vp(interpolated_sample_pos_vs, vp_size, g_proj);

			if(length(interpolated_sample_pos_vp - IN.position.xy) < 5)
			{
				return GREEN;
			}
								
			float2 interpolated_sample_pos_uv = vp_to_uv(vp_size, interpolated_sample_pos_vp);			
			
			//return 0;
		}
		
		if(length(float2(s_x_ss, s_y_ss) - IN.position.xy) < 5)
		{
			return RED * s_z_error_vs + BLUE * -s_z_error_vs;
		}
		
		last_test_t = t;
		last_test_z_vs_err = s_z_error_vs;
	}
	return 0;
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

	//idea: increment in small steps the further away the ray is
	float4 result = 0;
	for(int i = 0; i < max(vp_size.x, vp_size.y) / base_increment; i++)
	{
		//we can't increment this directly...
		//we have to scale it so that an increment will cross approximately 1 pixel
		float increment = base_increment;//(base_increment + i) * clamp(abs(last_test_z_vs_err) / 10, .7, 1.5);
		
		s_y_ss += increment * r_ss.y;
		s_x_ss += increment * r_ss.x; 
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

		//we can't bilinearly sample Z
		//float s_actual_z_ndc = g_depth.SampleGrad (g_linear, s_pos_uv, 0, 0);
		float s_actual_z_ndc = g_depth.Load(int3(s_x_ss, s_y_ss, 0));
		//if we Load ndc Z, we can't interpolate based on this

		float s_actual_z_vs = unproject_z(s_actual_z_ndc, g_proj_constants.xy);
		float s_desired_z_vs = unproject_z(s_desired_z_ndc, g_proj_constants.xy);
		float s_z_error_vs = s_actual_z_vs - s_desired_z_vs;
		
		//(sign(s_z_error_vs) != sign(last_test_z_vs_err))
		//the above doesnt work because it also succeeds when the ray poke out of something

		if(
			abs(s_z_error_vs) < z_error_bounds && //current error is acceptable
			abs(last_test_z_vs_err < z_error_bounds) && //last error is acceptable
			(last_test_z_vs_err > 0) &&
			(s_z_error_vs < 0)
			
			)
		{
			float err_ratio = abs(last_test_z_vs_err) / (abs(s_z_error_vs) + abs(last_test_z_vs_err));
			
			float3 last_test_vs_pos = pos_vs + last_test_t * r_vs;
			float3 current_test_vs_pos = pos_vs + t * r_vs;

			float3 interpolated_sample_pos_vs = lerp(last_test_vs_pos, current_test_vs_pos, err_ratio);				

			float2 interpolated_sample_pos_vp = vs_to_vp(interpolated_sample_pos_vs, vp_size, g_proj);
								
			float2 interpolated_sample_pos_uv = vp_to_uv(vp_size, interpolated_sample_pos_vp);			
			
			//return dot(-g_normal.SampleGrad(g_linear, interpolated_sample_pos_uv, 0, 0).xyz, r_vs);
			//return dot(-g_normal.SampleGrad(g_linear, interpolated_sample_pos_uv, 0, 0).xyz, dir_vs);
			result = float4(g_albedo.SampleGrad(g_linear, interpolated_sample_pos_uv, 0, 0), 1);
		}
		
		last_test_t = t;
		last_test_z_vs_err = s_z_error_vs;
	}
	float4 d = debug(IN);
	if(length(d) > 0) return d;
	if(length(result) > 0) return result;
	return float4(g_albedo.Load(pix_coord, 0).xyz, 1);

}