#include "shader.h"

uint2 packf4(float4 v)
{
	return uint2(
		f32tof16(v.x) | (f32tof16(v.y) << 16),
		f32tof16(v.z) | (f32tof16(v.w) << 16));
}
float4 unpackf4(uint2 v)
{
	return float4(
		f16tof32(v.x),
		f16tof32(v.x >> 16),
		f16tof32(v.y),
		f16tof32(v.y >> 16));
}
//use odd entries
cbuffer DDofCB
{
	//x = focal plane
	//y = # iterations
	//z = passidx
	float4 g_ddofVals;
	//x,y = image width/height
	float4 g_ddofVals2;
};
cbuffer FSQuadCB : register(b1)
{	
	float4x4 g_inv_p;
	float4 g_proj_constants;
	float4 g_debug_vars;
	float4x4 g_proj;
};

/* depth of field stuff */
float beta(float coc, int iterations)
{
	return coc * coc * (1.f/iterations);
}
float z2coc(float sampleZ, float aperture, float focalLength, float focalPlane)
{
	//from http://http.developer.nvidia.com/GPUGems/gpugems_ch23.html
	return abs(aperture * (focalLength * (sampleZ - focalPlane)) /
		(sampleZ * (focalPlane - focalLength)));

}
//get beta, and return 0 if xy.x is out of bounds
float betaX(Texture2D<float> depthTex, int2 xy, int2 size)
{	
	float focalPlane = g_ddofVals.x;
	float iterations = g_ddofVals.y;
	float2 projConstants = g_proj_constants.xy;

	return ((xy.x > -1) && (xy.x < size.x)) * 
		beta(z2coc(unproject_z(depthTex[xy], projConstants), 100, .5, focalPlane), iterations);
}
struct ABCDEntry
{
	float a;
	float b;
	float c;
	float3 d;
};

void ddofUnpack(uint4 input, out float a, out float b, out float c, out float3 d)
{
	//works
	d = float3(
		f16tof32(input.x), 
		f16tof32(input.x >> 16), 
		f16tof32((input.y & 0x3f) | ((input.z & 0x3f) << 6 ) | ((input.w & 0x3f) << 12 ))
		);
	
	a = asfloat(input.y & 0xffffffc0);
	b = asfloat(input.z & 0xffffffc0);
	c = asfloat(input.w & 0xffffffc0);

}
ABCDEntry ddofUnpack(uint4 input)
{
	ABCDEntry abcd;
	ddofUnpack(input, abcd.a, abcd.b, abcd.c, abcd.d);
	return abcd;
}
uint4 ddofPack(float a, float b, float c, float3 d)
{
	return uint4(
		asuint(f32tof16(d.x) | (f32tof16(d.y) << 16)),
		(asuint(a) & 0xffffffc0) | f32tof16(d.z) & 0x3F,
		(asuint(b) & 0xffffffc0) | (f32tof16(d.z) >> 6) & 0x3F,
		(asuint(c) & 0xffffffc0) | (f32tof16(d.z) >> 12) & 0x3F
		);
}
//works
int coordNOffset(int i, int passIdx)
{
	//i*2^(p+1)+2^(p+1)-2+1
	int k = pow(2, passIdx+1);
	return i * k + k - 2 + 1;
}

//works
int2 calcXYDelta(int2 rawXyDelta, int passIdx)
{
	return rawXyDelta * pow(2, passIdx + 1);
}

struct ABCDTriple
{
	float3 a;
	float3 b;
	float3 c;
	float3x3 d;
};
ABCDTriple readABCD(Texture2D<uint4> abcdTex, int2 xy, int2 xyDelta)
{
	//out of bounds = read 0
	int2 xy0 = xy - xyDelta;
	int2 xy1 = xy;
	int2 xy2 = xy + xyDelta;
	ABCDTriple abcd;
	ddofUnpack(abcdTex[xy0], abcd.a[0], abcd.b[0], abcd.c[0], abcd.d[0]);
	ddofUnpack(abcdTex[xy1], abcd.a[1], abcd.b[1], abcd.c[1], abcd.d[1]);
	ddofUnpack(abcdTex[xy2], abcd.a[2], abcd.b[2], abcd.c[2], abcd.d[2]);
	
	return abcd;
}
ABCDTriple computeABCD(Texture2D<float> depthTex, Texture2D<float3> colorTex, int2 xy, int2 xyDelta, int2 size,				  
				  float2 projConstants, float focalPlane, int iterations)
{
	ABCDTriple abcd;

	float betaPrev = betaX(depthTex, xy - 2 * xyDelta, size);
	float betaCur = betaX(depthTex, xy - xyDelta, size);
	float betaNext = 0;
	float prevC = -min(betaPrev, betaCur);
	[unroll]
	for(int i = -1; i < 2; i++)
	{		
		betaNext = betaX(depthTex, xy + (i + 1) * xyDelta, size);
		float betaForward = min(betaCur, betaNext);

		int idx = i+1;
		
		abcd.a[idx] = prevC;
		abcd.b[idx] = 1 - prevC + betaForward;
		float c = -betaForward;
		abcd.c[idx] = c;
		abcd.d[idx] = colorTex[xy + i * xyDelta];
		prevC = c;
		betaPrev = betaCur;
		betaCur = betaNext;		
	}
	return abcd;
}

ABCDEntry computeABCDEntry(Texture2D<float> depthTex, 
							Texture2D<float3> colorTex, int2 xy, int2 xyDelta, int2 size,				  
				  float2 projConstants, float focalPlane, int iterations)
{
	ABCDEntry abcd;

	float betaPrev = betaX(depthTex, xy - xyDelta, size);
	float betaCur = betaX(depthTex, xy, size);
	float betaNext = betaX(depthTex, xy + xyDelta, size);
	
	float betaBackward = min(betaPrev, betaCur);
	float betaForward = min(betaCur, betaNext);

	abcd.a = -betaBackward;
	abcd.b = 1 + betaBackward + betaForward;
	abcd.c = -betaForward;
	abcd.d = colorTex[xy];

	return abcd;
}

ABCDEntry reduce(ABCDTriple abcd3)
{	
	ABCDEntry abcd;
	
	float m0 = abcd3.b[0] == 0 ? 0 : -(abcd3.a[1] / abcd3.b[0]);
	float m1 = abcd3.b[2] == 0 ? 0 : -(abcd3.c[1] / abcd3.b[2]);

	abcd.a = m0 * abcd3.a[0];
	abcd.b = abcd3.b[1] + m0 * abcd3.c[0] + m1 * abcd3.a[2];
	abcd.c = m1 * abcd3.c[2];
	//todo: take care of endpoints!
	//i think it does at the moment, but confirm
	abcd.d = abcd3.d[1] + m0 * abcd3.d[0] + m1 * abcd3.d[2];
	return abcd;
}