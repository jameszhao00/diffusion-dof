cbuffer ObjectCB
{
	float4x4 wvp;
};
struct App2VS
{
	float4 position : POSITION;
};
struct VS2PS
{
	float4 position : SV_POSITION;
};
VS2PS vs(App2VS IN )
{
	VS2PS OUT;
	OUT.position = mul(IN.position, wvp);
    return OUT;
}
float4 ps( VS2PS IN ) : SV_Target
{
    return float4( 1.0f, 1.0f, 0.0f, 1.0f );    // Yellow, with Alpha = 1
}