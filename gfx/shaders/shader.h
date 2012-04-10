#define MSAA_COUNT 1
#define DEPTH_MAX 0
#define DEPTH_MIN 1

#define MAX_BONES 64

#define RED float4(1, 0, 0, 1)
#define GREEN float4(0, 1, 0, 1)
#define BLUE float4(0, 0, 1, 1)
#define BACK float3(0, 0, -1)

float unproject_z(float z, float2 proj_constants)
{
	return proj_constants[1] / (z - proj_constants[0]);
}
float3 get_vs_pos(float3 vs_ray, float ndc_z, float2 proj_constants)
{
	return vs_ray * unproject_z(ndc_z, proj_constants);
}

/*
//n1/n2 = ndc bounds...
float2 ndc_to_vp(float2 n1, float2 n2, float2 vp, float2 n)
{
	float2 v;
	v.x = (n.x - n1.x) / (n2.x - n1.x) * vp.x;
	v.y = (1 - (n.y - n2.y) / (n1.y - n2.y)) * vp.y;
	return v;
}
*/
float2 ndc_to_vp(float2 vp_size, float3 ndc)
{
	float2 v;
	v.x = .5 * vp_size.x * (ndc.x + 1);
	v.y = (.5 - ndc.y / 2) * vp_size.y;
	return v;
}

float2 vp_to_uv(float2 vp_size, float2 p)
{
	float2 v;
	v.x = -1 + 2 * p.x / vp_size.x;
	v.y = 1 - 2 * p.y / vp_size.y;
	return v;
}

float2 vs_to_vp(float3 vs_pos, float2 vp_size, float4x4 proj)
{
	float4 r = mul(float4(vs_pos, 1), proj);
	float3 ndc = r.xyz / r.w;
	return ndc_to_vp(vp_size, ndc.xyz);
}

float luminance(float3 source)
{
	return 0.2126 * source.r + 0.7152 * source.g + 0.0722 * source.b;
}