Texture2D<float3> g_bc;
Texture2D<float3> g_d;
//for debugging
Texture2D<float3> g_color;

RWTexture2D<float4> g_output;

cbuffer DDofCB
{
	float4 g_coc;
};

[numthreads(32, 1, 1)]
void csPass2H( uint3 DTid : SV_DispatchThreadID )
{
	float2 size;
	//work horizontally.. very inefficient
	g_bc.GetDimensions(size.x, size.y);
	
	//depends on primary direction
	float primarySize = size.y;
	float secondarySize = size.x;

	if(DTid.x > (primarySize - 1)) return; //we're out of bounds
	
	//solve for last row's b
	//depends on primary direction
	int2 coordLast = int2(secondarySize - 1, DTid.x);
	float bN = g_bc[coordLast].y;
	float3 dN = g_d[coordLast];
	float3 xN = dN / bN;
	g_output[coordLast] = float4(xN, 1);
	float3 xNPlusTwo = 0;
	float3 xNPlusOne = 0;
	//loop
	for(int i = secondarySize - 2; i > -1; i--)
	{
		xNPlusTwo = xNPlusOne;
		xNPlusOne = xN;
		//depends on primary direction
		int2 coordN = int2(i, DTid.x);
		bN = g_bc[coordN].y;
		float cN = g_bc[coordN].z;
		dN = g_d[coordN];
		xN = (dN - xNPlusOne * cN) / bN;	
		//HACK: debug
		g_output[coordN] = float4(xN, 1);
		
		//validate using xN, xNPlusOne, and xNPlusTwo
		{
			//xN * a + xNPlusOne * b + xNPlusTwo * c == dNPlusOne
			float3 dNPlusOne = g_color[coordN + int2(1, 0)];
			float3 result = xN * -1 * g_coc.x + xNPlusOne * 3 * g_coc.x + xNPlusTwo * -1 * g_coc.x;
			if(dot(result, dNPlusOne) > 0.00000001)
			{
				g_output[coordN] = float4(1, 0, 0, 1);
			}
		}
		
		
	}
}

[numthreads(32, 1, 1)]
void csPass2V( uint3 DTid : SV_DispatchThreadID )
{
	float2 size;
	//work horizontally.. very inefficient
	g_bc.GetDimensions(size.x, size.y);
	
	//depends on primary direction
	float primarySize = size.x;
	float secondarySize = size.y;

	if(DTid.x > (primarySize - 1)) return; //we're out of bounds
	
	//solve for last row's b
	//depends on primary direction
	int2 coordLast = int2(DTid.x, secondarySize - 1);
	float bN = g_bc[coordLast].y;
	float3 dN = g_d[coordLast];
	float3 xN = dN / bN;
	g_output[coordLast] = float4(xN, 1);
	//loop
	for(int i = secondarySize - 2; i > -1; i--)
	{
		float3 xNPlusOne = xN;
		//depends on primary direction
		int2 coordN = int2(DTid.x, i);
		bN = g_bc[coordN].y;
		float cN = g_bc[coordN].z;
		dN = g_d[coordN];
		xN = (dN - xNPlusOne * cN) / bN;		
		g_output[coordN] = float4(xN, 1);
	}
}
