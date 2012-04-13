#include "shader.h"
SamplerState g_linear : register(s[0]);
SamplerState g_aniso : register(s[1]);

#if MSAA_COUNT > 1
Texture2DMS<float4, MSAA_COUNT> g_normal : register(t[0]);
Texture2DMS<float3, MSAA_COUNT> g_albedo : register(t[1]);
Texture2DMS<float, MSAA_COUNT> g_depth : register(t[2]);
#else
Texture2D<float4> g_normal : register(t[0]);
Texture2D<float3> g_albedo : register(t[1]);
Texture2D<float> g_depth : register(t[2]);
Texture2D<float3> g_noise: register(t[3]);
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

float2 raytrace_fb(float2 pix_coord, float2 vp_size, float3 pos_vs, float3 r_vs, float half_cot_fov, out float target_t)
{	
	target_t = -1;
	float2 r_ss = normalize(vs_to_vp(pos_vs + r_vs, vp_size, g_proj) - pix_coord);
	
	//idea: increment in small steps the further away the ray is
	float4 result = 0;
	
	float2 s_pos_ss = pix_coord.xy;

	float2 target_vp = -1;
	float min_t = 0;
	float2 min_pos_vp = s_pos_ss;
	float max_t = -1;
	float2 max_pos_vp = -1;
	bool detailed_search = false;
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
	return target_vp;
}
#define CACHE_POINTS_SEPARATION 10
float4 gen_samples_ps(VS2PS IN) : SV_TARGET
{
	uint2 vp_size;
	g_normal.GetDimensions(vp_size.x, vp_size.y);

	float3 pix_coord = float3(floor(IN.position.xy / CACHE_POINTS_SEPARATION) * CACHE_POINTS_SEPARATION + 0.5, 0);
	float patch_sample_i = (IN.position.y - pix_coord.y) * CACHE_POINTS_SEPARATION + (IN.position.x - pix_coord.x);

	//if(patch_sample_i == 0) return RED;

	float ndc_depth = g_depth.Load(pix_coord, 0);
	float3 dir_vs = get_vs_ray(vp_size, pix_coord.xy, g_inv_p);
	float3 pos_vs = get_vs_pos(dir_vs, ndc_depth, g_proj_constants.xy);
	
	float3 n_vs = normalize(g_normal.Load(pix_coord, 0).xyz);

	float3 r_vs = reflect(dir_vs, n_vs);
	float half_cot_fov = g_debug_vars[0];
	float4 reflection_color = 0;
	float noise_intensity = 0.05;
	float3 noise = noise_intensity * normalize(g_noise.Load(float3(IN.position.xy % float2(256, 256), 0)).xyz - 0.5);
	r_vs = normalize(r_vs + noise);
	float target_t;
	float2 target_vp = raytrace_fb(pix_coord.xy, vp_size, pos_vs, r_vs, half_cot_fov, target_t);	
	if(target_t != -1) return g_albedo.SampleGrad(g_linear, vp_to_uv(vp_size, target_vp), 0, 0).xyzz;		
	else return 0;
}
float4 combine_samples_ps(VS2PS IN) : SV_TARGET
{		
	float2 base_pos_vp = (IN.position.xy - float2(0.5, 0.5)) * CACHE_POINTS_SEPARATION + float2(0.5, 0.5);

	Texture2D samples_tex = g_normal;

	float4 result = 0;
	for(int i = 0; i < CACHE_POINTS_SEPARATION; i++)
	{
		for(int j = 0; j < CACHE_POINTS_SEPARATION; j++)
		{
			float2 sample_pos_vp = base_pos_vp + float2(j, i);
			float4 sample_color = samples_tex.Load(float3(sample_pos_vp, 0));

			result += saturate(sample_color);
		}
	}
	//if(base_pos_vp.x < 500) return RED;
	//else return BLUE;
	//return (result / (CACHE_POINTS_SEPARATION * CACHE_POINTS_SEPARATION)) > 1 * RED;

	//return samples_tex.Load(float3(IN.position.xy, 0));
	return result / (CACHE_POINTS_SEPARATION * CACHE_POINTS_SEPARATION);
}
float4 shade_ps(VS2PS IN) : SV_TARGET
{
	{
		float3 pix_coord = float3(floor(IN.position.xy / CACHE_POINTS_SEPARATION) * CACHE_POINTS_SEPARATION + 0.5, 0);
		float patch_sample_i = (IN.position.y - pix_coord.y) * CACHE_POINTS_SEPARATION + (IN.position.x - pix_coord.x);
		//if(patch_sample_i == 0) return RED;
	}
	uint2 vp_size;
	g_normal.GetDimensions(vp_size.x, vp_size.y);
	Texture2D combined_samples_tex = g_normal;

	//return combined_samples_tex.Load(float3(IN.position.xy, 0));
	//return combined_samples_tex.Load(float3(IN.position.xy / CACHE_POINTS_SEPARATION, 0));
	
	float4 base_color = float4(g_albedo.Load(float3(IN.position.xy, 0)).xyz, 1);

	float2 uv = vp_to_uv(vp_size, IN.position.xy / CACHE_POINTS_SEPARATION);
	return 0.2 * combined_samples_tex.Sample(g_linear, uv) + 0.8 * base_color;
	return 1;
}
float4 ps(VS2PS IN) : SV_TARGET
{
	uint2 vp_size;
	g_normal.GetDimensions(vp_size.x, vp_size.y);
	float3 pix_coord = float3(IN.position.xy, 0);
	
	if(pix_coord.x < 8 && pix_coord.y < 8) return RED;
	//return g_albedo.SampleBias(g_linear, vp_to_uv(vp_size, IN.position), (IN.position.y / 600) * 5).xyzz;
	
	float4 base_color = float4(g_albedo.Load(pix_coord, 0).xyz, 1);
	//if((pix_coord.x - 0.5)%8 != 0 || (pix_coord.y - 0.5)%8 != 0) return base_color;

	float ndc_depth = g_depth.Load(pix_coord, 0);
	//cannot normalize viewspace ray!
	float3 pos_vs = get_vs_pos(IN.viewspace_ray, ndc_depth, g_proj_constants.xy);
	//if(ndc_depth == DEPTH_MAX) return 0;
	//be careful about the XYZ when normalizing!!!
	float3 dir_vs = normalize(IN.viewspace_ray.xyz);
	//sample view space normal vector
	float3 n_vs = normalize(g_normal.Load(pix_coord, 0).xyz);

	float3 r_vs = reflect(dir_vs, n_vs);
	float half_cot_fov = g_debug_vars[0];
	float4 reflection_color = 0;
	for(int i = 0; i < 1; i++)
	{		
		float3 noise = float3(0.08, 0.08, 0.08) * normalize(g_noise.Load(float3((IN.position.xy * float2(i+1, i+1)) % float2(256, 256), 0)).xyz);
		noise = 0;
		float3 sample_r_vs = normalize(r_vs + noise);
		float target_t;
		float2 target_vp = raytrace_fb(pix_coord.xy, vp_size, pos_vs, sample_r_vs, half_cot_fov, target_t);
		
		/*
		float3 normal = g_normal.Load(float3(target_vp, 0));
		float angle_blend = saturate(pow(dot(-normal.xyz, dir_vs), 3));
		float silhouette_blend = pow(saturate(dot(r_vs, -normal) + 0.57), 18);
		float blend = angle_blend * silhouette_blend;
		float distance_blend = 1 - pow(saturate(target_t / 30), 2);
		*/
		reflection_color += g_albedo.SampleGrad(g_linear, vp_to_uv(vp_size, target_vp), 0, 0).xyzz;		
	}

	reflection_color /= 1;
	float blend = .5;


	//if(target_vp.x != -1 && target_vp.y != -1)
	//{	
		return base_color * (1-blend) + blend * (saturate(reflection_color));
	//}
	//return color;
}