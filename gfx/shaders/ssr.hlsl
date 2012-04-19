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

static float poisson_12[] = {
	0.4364348f, -0.8602998f,
	-0.04865971f, -0.9352954f,
	0.5893933f, -0.4072268f,
	-0.3786264f, -0.4890129f,
	0.1309691f, -0.1763052f,
	0.04114918f, 0.4093734f,
	0.5093402f, 0.1355669f,
	-0.9850999f, -0.1390842f,
	-0.4856647f, -0.03864777f,
	-0.4714884f, 0.498933f,
	0.164139f, 0.8786861f,
	0.7568784f, 0.5255229f
};

cbuffer FSQuadCB : register(b[0])
{	
	float4x4 g_inv_p;
	float4 g_proj_constants;
	float4 g_debug_vars; //g_debug_vars[0] should have 0.5 * cot(fov)
	//NOT RLELEVANT! //debug vars[1]/[2] have x/y pos of the pixel to show rays for
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
	float2 r_ss = normalize(vs_to_vp(pos_vs + 50 * r_vs, vp_size, g_proj) - pix_coord);
	
	float2 p0_vp = pix_coord;
	//float2 p1_vp = p0_vp + 400 * r_ss;
	float2 p1_vp = vs_to_vp(pos_vs + 40 * r_vs, vp_size, g_proj);
	
	float z0 = pos_vs.z;
	//float end_t = vpy_to_t(p1_vp.y, half_cot_fov, vp_size, pos_vs, r_vs);
	//float z1 = (r_vs.z * end_t + pos_vs.z);
	float z1 = (pos_vs + 40 * r_vs).z;
	float z0_rcp = 1/z0;
	float z1_rcp = 1/z1;

	float increment = 0.08;
	float base = 0.02;
	float2 pos_vp_increment = (p1_vp - p0_vp) * increment;
	float ray_z_rcp_increment = (z1_rcp - z0_rcp) * increment;

	float2 sample_pos_vp = p0_vp + pos_vp_increment * (base / increment);
	float2 ray_z_rcp = z0_rcp + ray_z_rcp_increment * (base / increment);
	

	for(int i = 0; i < 33; i += 1)
	{		
		float actual_z = g_depth.Load(float3(sample_pos_vp, 0)).x * Z_FAR;
		float ray_z = rcp(ray_z_rcp);
		float z_error = actual_z - ray_z;
		if(actual_z < ray_z && abs(actual_z - ray_z) < 5) 
		{
			
			float3 final_pos = pos_vs + r_vs * vpy_to_t(sample_pos_vp.y, half_cot_fov, vp_size, pos_vs, r_vs);
			target_t = length(final_pos.xyz - pos_vs);
			return sample_pos_vp;
		}
		//don't need abs since actual_z will always be greater than ray_z to get this far
		float z_scale = clamp(z_error / 5, 0.4, 1);
		sample_pos_vp += pos_vp_increment * z_scale;
		ray_z_rcp += ray_z_rcp_increment * z_scale;
	}
	return 0;
	
}
float4 gen_samples_ps(VS2PS IN) : SV_TARGET
{
	uint2 vp_size;
	g_normal.GetDimensions(vp_size.x, vp_size.y);

	float3 pix_coord = float3(IN.position.xy, 0);

	float z_vs = g_depth.Load(pix_coord, 0) * Z_FAR;
	//return z_vs / 100;
	if(z_vs == Z_FAR) return float4(0, 0, 0, MAX_T);

	float3 dir_vs = normalize(IN.viewspace_ray);
	float3 pos_vs = IN.viewspace_ray * z_vs;
	
	float3 n_vs = normalize(g_normal.Load(pix_coord, 0).xyz);

	float3 r_vs = reflect(dir_vs, n_vs);
	float half_cot_fov = g_debug_vars[0];
	float4 reflection_color = 0;
	float noise_intensity = g_debug_vars[3];//0.09;
	float3 noise = noise_intensity * normalize(g_noise.Load(float3(IN.position.xy % float2(256, 256), 0)).xyz - 0.5);
	r_vs = normalize(r_vs + noise);

	float target_t;
	float2 target_vp = raytrace_fb(pix_coord.xy, vp_size, pos_vs, r_vs, half_cot_fov, target_t);	
	//return target_vp.x;
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
	float sample0_z = g_depth.Load(float3(pix_coord, 0)) * Z_FAR;

	float3 sample0_normal = g_normal.Load(float3(pix_coord, 0));
	float total_weights = 0;

	float blur_distance = g_debug_vars[2];
	float3 filtered = 0;
	float blur_scale = blur_distance;

	float rotation = (IN.position.y * vp_size.x + IN.position.x);
	float rotation_s = sin(rotation);
	float rotation_c = cos(rotation);
	
	float noise_intensity = g_debug_vars[3];
	for(int i = 0; i < 12; i++)
	{					
		float2 poisson_pt = float2(poisson_12[2 * i], poisson_12[2 * i + 1]);	

		poisson_pt.x = poisson_pt.x * rotation_c - poisson_pt.y * rotation_s;
		poisson_pt.y = poisson_pt.x * rotation_s + poisson_pt.y * rotation_c;


		float2 pix_offset = poisson_pt * blur_distance * (35 / sample0_z);
		float2 uv_offset = pix_offset * pix_size_uv;

		float4 sample = samples_tex.Load(float3(pix_coord + pix_offset, 0));			
		float sample_t = sample.w;

		//to fix blur edges where t is tiny and MAX_T is 3000...
		//also tried straight up decreasing MAX_T... 
		//think it sharpened the edges
		if(sample_t == MAX_T) sample_t = sample0_t;

		float3 color = sample.xyz;

		float sample_z = g_depth.Load(float3(pix_coord + pix_offset, 0)) * Z_FAR;
		float3 sample_normal = g_normal.Load(float3(pix_coord + pix_offset, 0)).xyz;

		//we want dot's range (-1, 1) to go to (0, 2)
		float normal_diff = abs(1 - dot(sample_normal, sample0_normal));
		normal_diff *= 2;
			
		float z_diff = clamp(abs(sample_z - sample0_z) - 3, 0, 1000);
		float diff = 10 * normal_diff + 2 * z_diff;			
			
		//ideally we want to separate the diff out
		float range = exp(-diff*diff/9);
		

		
		//float weight = gauss2d(abs(poisson_pt), blur_scale * clamp(sample_t / 100, 0, 6));
		float st = blur_scale * sample_t;
		//float weight = gauss2d(abs(poisson_pt), clamp(blur_scale * (sample_t)*(sample_t)/30000, 0.05, 100));
		float weight = gauss2d(abs(poisson_pt), clamp(blur_scale * (sample_t)*(sample_t)*(sample_t)/800000, 0.05, 10));

		if(g_vars[0] == 1) weight *= range;		

		total_weights += weight;			

		filtered += weight * saturate(sample);
					
	}
	filtered /= total_weights;
	return g_color.Load(float3(pix_coord, 0)).xyzz * .95 + filtered.xyzz * 0.05;

}