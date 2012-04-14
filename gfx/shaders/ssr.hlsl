#include "shader.h"
SamplerState g_linear : register(s[0]);
SamplerState g_aniso : register(s[1]);

#if MSAA_COUNT > 1
Texture2DMS<float4, MSAA_COUNT> g_normal : register(t[0]);
Texture2DMS<float3, MSAA_COUNT> g_albedo : register(t[1]);
Texture2DMS<float, MSAA_COUNT> g_depth : register(t[2]);
#else
Texture2D<float4> g_normal : register(t[0]);
Texture2D<float3> g_color : register(t[1]);
Texture2D<float> g_depth : register(t[2]);
Texture2D<float3> g_noise: register(t[3]);
Texture2D<float4> g_samples : register(t[4]);
#endif
#define z_error_bounds 25
#define base_increment 10
cbuffer FSQuadCB : register(b[0])
{	
	float4x4 g_inv_p;
	float4 g_proj_constants;
	float4 g_debug_vars; //g_debug_vars[0] should have 0.5 * cot(fov)
	//NOT LELEVANT! //debug vars[1]/[2] have x/y pos of the pixel to show rays for
	//debug vars[2] has gaussian multiplier
	//debug vars[3] has noise ratio
	float4 g_vars;
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
#define MAX_T 3000
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
		//cap ray walk dist
		if(t > 100) break;
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
float4 gen_samples_ps(VS2PS IN) : SV_TARGET
{
	uint2 vp_size;
	g_normal.GetDimensions(vp_size.x, vp_size.y);

	float3 pix_coord = float3(IN.position.xy, 0);

	float ndc_depth = g_depth.Load(pix_coord, 0);
	float3 dir_vs = get_vs_ray(vp_size, pix_coord.xy, g_inv_p);
	float3 pos_vs = get_vs_pos(dir_vs, ndc_depth, g_proj_constants.xy);
	
	float3 n_vs = normalize(g_normal.Load(pix_coord, 0).xyz);

	float3 r_vs = reflect(dir_vs, n_vs);
	float half_cot_fov = g_debug_vars[0];
	float4 reflection_color = 0;
	float noise_intensity = g_debug_vars[3];//0.09;
	float3 noise = noise_intensity * normalize(g_noise.Load(float3(IN.position.xy % float2(256, 256), 0)).xyz - 0.5);
	r_vs = normalize(r_vs + noise);
	float target_t;
	float2 target_vp = raytrace_fb(pix_coord.xy, vp_size, pos_vs, r_vs, half_cot_fov, target_t);	
	if(target_t != -1)
	{
		return float4(g_color.SampleGrad(g_linear, vp_to_uv(vp_size, target_vp), 0, 0).xyz, target_t);
	}
	else return float4(0, 0, 0, MAX_T);
}
float gauss(float x, float sigma)
{
	return exp(-x*x/(sigma * sigma));
}
float gauss2d(float2 dp, float sigma)
{
	return exp(-dot(dp, dp)/(sigma * sigma));
}
float4 shade_ps(VS2PS IN) : SV_TARGET
{	
	uint2 vp_size;
	g_normal.GetDimensions(vp_size.x, vp_size.y);
	float2 pix_coord = IN.position.xy;

	const Texture2D samples_tex = g_samples;
	float2 uv = vp_to_uv(vp_size, pix_coord);
	float2 pix_size_uv = vp_pix_uv(vp_size);
	float4 sample0 = samples_tex.Sample(g_linear, uv);
	float3 sample0_color = sample0.xyz;
	float sample0_t = sample0.w;
	float sample0_z = unproject_z(g_depth.Load(float3(pix_coord, 0)), g_proj_constants);
	float3 sample0_normal = g_normal.Load(float3(pix_coord, 0));
	float total_weights = 0;
	float blur_distance = g_debug_vars[2];
	float3 filtered = 0;
	float blur_scale = 1;//clamp(sample0_t, 0,28) / 28;

	for(int i = -3; i < 4; i++)
	{
		for(int j = -3; j < 4; j++)
		{		
			
			float2 offset = float2(i, j) * blur_distance;
			float4 sample = samples_tex.Sample(g_linear, uv + offset * pix_size_uv);
			float sample_t = sample.w;

			//to fix blur edges where t is tiny and MAX_T is 3000...
			//also tried straight up decreasing MAX_T... 
			//think it sharpened the edges
			if(sample_t == MAX_T) sample_t = sample0_t;

			float3 color = sample.xyz;

			float sample_z = unproject_z(g_depth.Load(float3(pix_coord + offset, 0)), g_proj_constants.xy);	
			float3 sample_normal = g_normal.Load(float3(pix_coord + offset, 0));

			//we want dot's range (-1, 1) to go to (0, 2)
			float normal_diff = abs(1 - dot(sample_normal, sample0_normal));
			normal_diff *= 2;
			
			float z_diff = clamp(abs(sample_z - sample0_z) - 3, 0, 1000);
			float diff = 10 * normal_diff + 2 * z_diff;			
			
			//ideally we want to separate the diff out
			float range = exp(-diff*diff/9);
			
			float weight = gauss2d(abs(float2(i, j)), blur_scale * clamp(sample_t / 15, 0, 3));
			
			if(g_vars[0] == 1) weight *= range;		

			total_weights += weight;			

			filtered += weight * saturate(sample);

		}
	}

	filtered /= total_weights;// == 0 ? 1 : total_weights;
	float3 total = filtered;
	//return sample0_t != MAX_T ? sample0_t / 30 : 0;
	//return (total < 0).xyzz;
	return g_color.Load(float3(pix_coord, 0)).xyzz * .95 + total.xyzz * 0.05;

	//return .99 * g_color.Load(float3(pix_coord, 0)).xyzz + filtered.xyzz / total_weights * 0.01;// * BLUE;
	//return .05 * filtered.xyzz / total_weights + .95 * g_color.Load(float3(pix_coord, 0)).xyzz;
}