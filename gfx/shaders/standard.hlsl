cbuffer ObjectCB
{
	float4x4 wvp;
};
struct App2VS
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 uv : UV;
};
struct VS2PS
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : UV;
};
VS2PS vs(App2VS IN )
{
	VS2PS OUT;
	OUT.position = mul(float4(IN.position, 1), wvp);
	OUT.normal = IN.normal;
	OUT.uv = IN.uv;
    return OUT;
}
float4 ps( VS2PS IN ) : SV_Target
{
	float ndotl = dot(IN.normal, normalize(float3(1, 1, 1)));
    return float4( ndotl, ndotl, ndotl, 1.0f );    // Yellow, with Alpha = 1
}