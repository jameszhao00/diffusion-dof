Texture2D<float4> g_normal : register(t[0]);
Texture2D<float> g_depth : register(t[1]);
Texture2D<float3> g_albedo : register(t[2]);
Texture2D<float4> g_debug : register(t[3]);

Texture2D<float4> g_ndc_tri_verts0 : register(t[4]);
Texture2D<float4> g_ndc_tri_verts1 : register(t[5]);
Texture2D<float4> g_ndc_tri_verts2 : register(t[6]);
cbuffer FSQuadCb
{
	float4x4 g_inv_p;
	float4 g_proj_constants;
	float4 g_debug_vars;
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

//some random barycentric pt in triangle code
//from http://blogs.msdn.com/b/rezanour/archive/2011/08/07/barycentric-coordinates-and-point-in-triangle-tests.aspx	
bool point_in_tri(float3 pt, float3 tri[3])
{
	const float epsilon = 0;//0.00001;
	float3 a = tri[0];
	float3 b = tri[1];
	float3 c = tri[2];
	float3 u = b-a;
	float3 v = c-a;
	float3 w = pt-a;

	float3 vcw = cross(v, w);
	float3 vcu = cross(v, u);
	if(dot(vcw, vcu) < -epsilon) return false;

	float3 ucw = cross(u, w);
	float3 ucv = cross(u, v);
	if(dot(ucw, ucv) < -epsilon) return false;

	float denom = length(ucv);
	float r = length(vcw) / denom;
	float t = length(ucw) / denom;

	return (r <= (1 + epsilon)) && (t <= (1 + epsilon)) && (r + t <= (1 + epsilon));

}
//n1 = ndc top left
//n2 = ndc bottom right
//vp = vp size
//n = ndc pos
float2 ndc_to_vp(float2 n1, float2 n2, float2 vp, float2 n)
{
	float2 v;
	v.x = (n.x - n1.x) / (n2.x - n1.x) * vp.x;
	v.y = (1 - (n.y - n2.y) / (n1.y - n2.y)) * vp.y;
	return v;
}
float2 vp_to_ndc(float2 vp, float2 p)
{
	float2 v;
	v.x = (p.x / vp.x) * 2 - 1;
	v.y = (1 - p.y / vp.y) * 2 - 1;
	return v;
}
float4 ps(VS2PS IN) : SV_TARGET
{
	const float4 red = float4(1, 0, 0, 1);
	const float4 green = float4(0, 1, 0, 1);
	const float4 blue = float4(0, 0, 1, 1);

	

	bool aa_visualize = g_debug_vars.y > 0;
	float3 pix_coord = float3(IN.position.xy, 0);
	float depth = g_depth.Load(pix_coord, 0);
	float4 debug = g_debug.Load(pix_coord, 0);
	float3 albedo = g_albedo.Load(pix_coord, 0);
	return albedo.xyzz;
	//debug visualization
	float2 debug_vp_point = g_debug_vars.zw;

	if((abs(IN.position.x - debug_vp_point.x) < 5) && 
		(abs(IN.position.y - debug_vp_point.y) < 5))
	{
		return red;
	}
	//fast path... for when its not an edge
	if((debug.x == 0) && !aa_visualize) return albedo.xyzz;



	//ndc ranges from -1,1... so mul by 2
	uint2 vp_size;
	//uint sample_count;
	g_normal.GetDimensions(vp_size.x, vp_size.y);
	float2 ndc_pixel_size = 1.0 / vp_size * 2.0; 
			
	bool color_bar = (IN.position.y / vp_size.y) > 0.95;

	float2 vp_pix_top_left = IN.position.xy - float2(0.5, 0.5);
	float2 vp_pix_bottom_right = IN.position.xy + float2(0.5, 0.5);
	//float2 ndc_pix_top_left = ((vp_pix_top_left / vp_size) - 0.5) / 0.5 * float2(1, -1);
	//float2 ndc_pix_bottom_right = ((vp_pix_bottom_right / vp_size) - 0.5) / 0.5 * float2(1, -1);
	float2 ndc_pix_top_left = vp_to_ndc(vp_size, vp_pix_top_left);
	float2 ndc_pix_bottom_right = vp_to_ndc(vp_size, vp_pix_bottom_right);

	if(aa_visualize == true)
	{		
		//float2 vp_point = float2(400, 300);
		pix_coord = int3(debug_vp_point, 0);
		ndc_pix_top_left = vp_to_ndc(vp_size, debug_vp_point);
		ndc_pix_bottom_right = vp_to_ndc(vp_size, debug_vp_point + float2(1, 1));
	}
	const float d_max = -0.001;
	const float d_min = 1.001;

	//int used_tri_ids[9]; //9 b/c we don't want to do conditional writes
	//for(int i = 0; i < 9; i++) used_tri_ids[i] = -1;
	float3 tri_verts[9][3];
	float3 tri_albedos[9];
	float tri_depths[9];
	for(int i = 0; i < 9; i++) tri_depths[i] = d_max;

	for(int off_x = -1; off_x < 2; off_x++)
	{
		for(int off_y = -1; off_y < 2; off_y++)
		{
			int off_linear_index = 3 * (off_x + 1) + (off_y + 1);
			int3 offset_pix_coord = pix_coord + int3(off_x, off_y, 0);
			float4 tri_info = g_ndc_tri_verts0.Load(offset_pix_coord);
			float tri_id = tri_info.w;
			float3 tri_albedo = g_albedo.Load(offset_pix_coord, 0);

			float3 tri_vert0 = tri_info.xyz / 0.8;

			float3 tri_vert1 =
				g_ndc_tri_verts1.Load(offset_pix_coord).xyz / 0.8;
			float3 tri_vert2 = 
				g_ndc_tri_verts2.Load(offset_pix_coord).xyz / 0.8;		
			{
				tri_vert0.z = 0;
				tri_vert1.z = 0;
				tri_vert2.z = 0;
			}
			tri_verts[off_linear_index][0] = tri_vert0;
			tri_verts[off_linear_index][1] = tri_vert1;
			tri_verts[off_linear_index][2] = tri_vert2;

			tri_albedos[off_linear_index] = g_albedo.Load(offset_pix_coord);
			tri_depths[off_linear_index] = g_depth.Load(offset_pix_coord);
		}
	}

	//for debug visualization of a single viewport pixel
	float4 debug_color = float4(0, 0, .15, 0);
	float4 debug_composite = 0;
	if(aa_visualize == true)
	{
		float2 n1 = ndc_pix_top_left;
		float2 n2 = ndc_pix_bottom_right;
		float2 vp = vp_size;
		float4 color = 0;
		float tri_count = 0;
		float closest_depth = d_max;
		float4 closest_color = 0;

		for(int i = 0; i < 9; i++)
		{			
			float2 ndc_tri[3];
			ndc_tri[0] = tri_verts[i][0];
			ndc_tri[1] = tri_verts[i][1];
			ndc_tri[2] = tri_verts[i][2];
			float3 vp_tri[3];
			vp_tri[0] = float3(ndc_to_vp(n1, n2, vp, ndc_tri[0]), 0);
			vp_tri[1] = float3(ndc_to_vp(n1, n2, vp, ndc_tri[1]), 0);
			vp_tri[2] = float3(ndc_to_vp(n1, n2, vp, ndc_tri[2]), 0);
			bool pt_in_tri = point_in_tri(float3(IN.position.xy, 0), vp_tri);
			float tri_depth = tri_depths[i];
			//inverted depth
			if(pt_in_tri && (tri_depth > closest_depth))
			{
				closest_depth = tri_depth;
				closest_color = debug_color + (i + 1) / 10.0 * blue;
			}
		}

		debug_composite += closest_color;
	}

	float3 color = 0;
	int total_samples = 0;
	float2 ndc_incrementer = float2(ndc_pixel_size.x / 4, ndc_pixel_size.y / 4);
	
	//normalization factor
	//for subpixel triangles that aren't stored
	//otherwise we get dark spots...
	float nohit_samples = 0;

	for(float i = ndc_pix_top_left.x + ndc_incrementer.x / 2; 
		i < ndc_pix_bottom_right.x; 
		i += ndc_incrementer.x)
	{
		for(float j = ndc_pix_bottom_right.y + ndc_incrementer.y / 2; 
			j < ndc_pix_top_left.y; 
			j += ndc_incrementer.y)
		{
			float3 sample_point = float3(i, j, 0);
				
			float closest_depth = d_max;
			float3 closest_albedo = 0;
			total_samples++;
			bool debug_covered = false;
			for(int tri_i = 0; tri_i < 9; tri_i++)
			{
				float3 ndc_tri_verts[3];
				ndc_tri_verts[0] = tri_verts[tri_i][0];
				ndc_tri_verts[1] = tri_verts[tri_i][1];
				ndc_tri_verts[2] = tri_verts[tri_i][2];
				
				float tri_depth = tri_depths[tri_i];
				bool sample_in_tri = point_in_tri(sample_point, ndc_tri_verts);
				bool use_sample = 
					sample_in_tri &&
					(tri_depth > closest_depth);


				closest_depth = closest_depth * (1 - use_sample) 
					+ tri_depth * (use_sample);
				closest_albedo = closest_albedo * (1 - use_sample) 
					+ tri_albedos[tri_i] * (use_sample);
			}

			if(aa_visualize)
			{
				float2 sample_ndc = float2(i, j);
					
				float2 n1 = ndc_pix_top_left;
				float2 n2 = ndc_pix_bottom_right;
				
				float2 vp = vp_size;
				float2 sample_vp = ndc_to_vp(n1, n2, vp, sample_ndc);
				if(
					(abs(sample_vp - IN.position).x < 10) &&
					(abs(sample_vp - IN.position).y < 10)
					)
				{
					bool hit = (closest_depth > d_max);
					debug_composite = hit * green + !hit * red;
				}
			}

			color += closest_albedo;
			if(closest_depth == d_max)
			{
				nohit_samples ++;
			}
		}
	}
	float normalization = total_samples / (total_samples - nohit_samples);
	
	if(aa_visualize)
	{
		return debug_composite;
	}
	return normalization * color.xyzz / total_samples;

}