Texture2D<float3> g_bc;
Texture2D<float3> g_d;
//for debugging
Texture2D<float3> g_color;

RWTexture2D<float4> g_output;

cbuffer DDofCB
{
	float4 g_coc;
};
void ddofPass2(int2 coord0, int2 coordDelta, int numItems)
{
	int2 coordLast = coord0 + (numItems - 1) * coordDelta;

	float bN = g_bc[coordLast].y;
	float3 dN = g_d[coordLast];
	float3 xN = dN / bN;
	g_output[coordLast] = float4(xN, 1);
	float3 xNPlusTwo = 0;
	float3 xNPlusOne = 0;
	//loop
	for(int i = numItems - 2; i > -1; i--)
	{
		xNPlusTwo = xNPlusOne;
		xNPlusOne = xN;
		//depends on primary direction
		int2 coordN = coord0 + i * coordDelta;
		bN = g_bc[coordN].y;
		float cN = g_bc[coordN].z;
		dN = g_d[coordN];
		xN = (dN - xNPlusOne * cN) / bN;	
		g_output[coordN] = float4(xN, 1);
	}
}
[numthreads(32, 1, 1)]
void csPass2H( uint3 DTid : SV_DispatchThreadID )
{
	float2 size;
	//work horizontally.. very inefficient
	g_bc.GetDimensions(size.x, size.y);
	
	if(DTid.x > (size.y - 1)) return; //we're out of bounds

	ddofPass2(int2(0, DTid.x), int2(1, 0), size.x);
}

[numthreads(32, 1, 1)]
void csPass2V( uint3 DTid : SV_DispatchThreadID )
{
	float2 size;
	//work horizontally.. very inefficient
	g_bc.GetDimensions(size.x, size.y);
	
	if(DTid.x > (size.x - 1)) return; //we're out of bounds

	ddofPass2(int2(DTid.x, 0), int2(0, 1), size.y);
}
