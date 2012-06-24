
Texture2D<uint4> g_bcd;

RWTexture2D<float4> g_output;

cbuffer DDofCB
{
	float4 g_coc;
};
void ddofUnpack(uint4 input, out float b, out float c, out float3 d)
{
	d = float3(f16tof32(input.x), f16tof32(input.x >> 16), asfloat(input.y));
	b = asfloat(input.z);
	c = asfloat(input.w);
}

void ddofPass2(int2 coord0, int2 coordDelta, int numItems)
{
	int2 coordLast = coord0 + (numItems - 1) * coordDelta;
	float cN = 0;
	float bN = 0;// = g_bc[coordLast].y;
	float3 dN = 0;// = g_d[coordLast];
	ddofUnpack(g_bcd[coordLast], bN, cN, dN);
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
					
		ddofUnpack(g_bcd[coordN], bN, cN, dN);
		
		xN = (dN - xNPlusOne * cN) / bN;	
		g_output[coordN] = float4(xN, 1);
	}
}
[numthreads(32, 1, 1)]
void csPass2H( uint3 DTid : SV_DispatchThreadID )
{
	float2 size;
	//work horizontally.. very inefficient
	g_bcd.GetDimensions(size.x, size.y);
	
	if(DTid.x > (size.y - 1)) return; //we're out of bounds

	ddofPass2(int2(0, DTid.x), int2(1, 0), size.x);
}

[numthreads(32, 1, 1)]
void csPass2V( uint3 DTid : SV_DispatchThreadID )
{
	float2 size;
	//work horizontally.. very inefficient
	g_bcd.GetDimensions(size.x, size.y);
	
	if(DTid.x > (size.x - 1)) return; //we're out of bounds

	ddofPass2(int2(DTid.x, 0), int2(0, 1), size.y);
}
