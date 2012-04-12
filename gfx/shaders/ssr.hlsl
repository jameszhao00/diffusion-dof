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
#define base_increment 10
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

float vpy_to_t(float vpy, float half_cot_fov, float2 vp_size, float3 pos_vs, float3 r_vs)
{
	//calculate the desired t. look at math.nb
	return
			(-half_cot_fov * vp_size.y * pos_vs.y + (-2 * vpy + vp_size.y) * pos_vs.z) / 
			(2 * vpy * r_vs.z - r_vs.z * vp_size.y + r_vs.y * half_cot_fov * vp_size.y);	
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

	//buffer by 1 r_ss
	float2 s_pos_ss = pix_coord.xy;
	if(length(s_pos_ss - IN.position.xy) < 5)
	{
		return GREEN;
	}
	float half_cot_fov = g_debug_vars[0];

	
	//idea: increment in small steps the further away the ray is
	float4 result = 0;

	//uv of the reflection point
	float2 target_vp = -1;
	float min_t = 0;
	float2 min_pos_vp = s_pos_ss;
	float max_t = -1;
	float2 max_pos_vp = -1;
	bool detailed_search = false;
	for(int i = 0; i < max(vp_size.x, vp_size.y) / base_increment; i++)
	{
		s_pos_ss += base_increment * (r_ss);
		if(!in_vp_bounds(s_pos_ss, vp_size)) break;

		float t = vpy_to_t(s_pos_ss.y, half_cot_fov, vp_size, pos_vs, r_vs);
		max_t = t;
		max_pos_vp = s_pos_ss;
		float s_desired_z_ndc = 
			(g_proj_constants[1] + g_proj_constants[0]*(r_vs.z * t + pos_vs.z)) / 
			(r_vs.z * t + pos_vs.z);

		float s_actual_z_ndc = g_depth.Load(int3(s_pos_ss, 0));
		
		if(closer_ndc(s_actual_z_ndc, s_desired_z_ndc))
		{			
			detailed_search = true;
			break;
		}
		if(abs(s_actual_z_ndc - s_desired_z_ndc) < 0.00005)
		{
			target_vp = s_pos_ss;
		}
		min_t = max_t;
		min_pos_vp = max_pos_vp;
	}
	if(detailed_search)
	{
		s_pos_ss = min_pos_vp;
		for(int i = 0; i < 500; i++)
		{
			s_pos_ss += r_ss;
			float t = vpy_to_t(s_pos_ss.yy, half_cot_fov, vp_size, pos_vs, r_vs);
			if(t > max_t) break; 
			float s_desired_z_ndc = 
				(g_proj_constants[1] + g_proj_constants[0]*(r_vs.z * t + pos_vs.z)) / 
				(r_vs.z * t + pos_vs.z);
				
			float s_actual_z_ndc = g_depth.Load(int3(s_pos_ss, 0));
			
		/****************************/
		float error = (s_actual_z_ndc - s_desired_z_ndc) * 10;
		if(length(s_pos_ss - IN.position.xy) < 1)
		{
			return RED * (error > 0) + BLUE * (error < 0);
			//return RED;
		}		
		/****************************/

			
			if(abs(s_actual_z_ndc - s_desired_z_ndc) < 0.00005)
			{
				/****************************/
				if(length(s_pos_ss - IN.position.xy) < 5)
				{
					return GREEN;
				}
				/****************************/
				target_vp = s_pos_ss;
				break;
			}
		}
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

	//buffer by 1 r_ss
	float2 s_pos_ss = pix_coord.xy;
	float half_cot_fov = g_debug_vars[0];

	
	//idea: increment in small steps the further away the ray is
	float4 result = 0;

	//uv of the reflection point
	float2 target_vp = -1;
	float min_t = 0;
	float2 min_pos_vp = s_pos_ss;
	float max_t = -1;
	float2 max_pos_vp = -1;
	bool detailed_search = false;
	float target_t = 0;
	for(int i = 0; i < 800 / base_increment; i++)
	{
		s_pos_ss += base_increment * (r_ss);
		if(!in_vp_bounds(s_pos_ss, vp_size)) break;

		float t = vpy_to_t(s_pos_ss.y, half_cot_fov, vp_size, pos_vs, r_vs);
		max_t = t;
		max_pos_vp = s_pos_ss;
		float s_desired_z_ndc = 
			(g_proj_constants[1] + g_proj_constants[0]*(r_vs.z * t + pos_vs.z)) / 
			(r_vs.z * t + pos_vs.z);

		float s_actual_z_ndc = g_depth.Load(int3(s_pos_ss, 0));
		
		if(abs(s_actual_z_ndc - s_desired_z_ndc) < 0.000001)
		{
			target_vp = s_pos_ss;
		}
		else if(closer_ndc(s_actual_z_ndc, s_desired_z_ndc))
		{			
			detailed_search = true;
			break;
		}
		min_t = max_t;
		min_pos_vp = max_pos_vp;
	}
	if(detailed_search)
	{
		s_pos_ss = min_pos_vp - r_ss * 2;
		max_t += 2;
		float previous_t = min_t;
		float previous_z_error_ndc = 1000;
		float previous_z_ndc = 0;
		for(int i = 0; i < 500; i++)
		{
			s_pos_ss += 2 * r_ss;
			float t = vpy_to_t(s_pos_ss.yy, half_cot_fov, vp_size, pos_vs, r_vs);
			if(t > max_t) break; 
			float s_desired_z_ndc = 
				(g_proj_constants[1] + g_proj_constants[0]*(r_vs.z * t + pos_vs.z)) / 
				(r_vs.z * t + pos_vs.z);
			
			float s_actual_z_ndc = g_depth.Load(int3(s_pos_ss, 0));
			float z_error_ndc = s_actual_z_ndc - s_desired_z_ndc;

			if((previous_z_error_ndc < 0) && (z_error_ndc > 0) && abs(previous_z_ndc - s_actual_z_ndc) < 0.0003)
			{
				//we've crossed the boundary
				float3 previous_ray_pos_vs = pos_vs + previous_t * r_vs;
				float3 current_ray_pos_vs = pos_vs + t * r_vs;
				float previous_actual_z_vs = unproject_z(previous_z_ndc, g_proj_constants.xy);
				float current_actual_z_vs = unproject_z(s_actual_z_ndc, g_proj_constants.xy);
				float error_ratio = abs(previous_actual_z_vs - previous_ray_pos_vs.z) / 
					(abs(previous_actual_z_vs - previous_ray_pos_vs.z) + abs(current_actual_z_vs - current_ray_pos_vs.z));

				float3 interpolated_sample_pos_vs = lerp(previous_ray_pos_vs, current_ray_pos_vs, error_ratio);
				float2 interpolated_sample_pos_vp = vs_to_vp(interpolated_sample_pos_vs, vp_size, g_proj);

				target_vp = interpolated_sample_pos_vp;
				target_t = t;
				break;
			}
			
			previous_z_error_ndc = z_error_ndc;
			previous_t = t;
			previous_z_ndc = s_actual_z_ndc;
		}
	}
	float3 normal = g_normal.Load(float3(target_vp, 0));
	float angle_blend = saturate(pow(dot(-normal.xyz, dir_vs), 3));
	float silhouette_blend = pow(saturate(dot(r_vs, -normal) + 0.57), 18);
	float blend = angle_blend * silhouette_blend;
	float distance_blend = 1 - pow(saturate(target_t / 30), 2);
	float mip_bias = 0;//(1 - distance_blend) * 3;
	//float4 d = debug(IN); if(length(d) > 0) return d;
	float4 color = float4(g_albedo.Load(pix_coord, 0).xyz, 1);

	if(target_vp.x != -1 && target_vp.y != -1)
	{	
		//we found something!		
		return color * (1-blend) + blend * float4(g_albedo.SampleBias(g_linear, vp_to_uv(vp_size, target_vp), mip_bias), 1);
	}
	return color;
}