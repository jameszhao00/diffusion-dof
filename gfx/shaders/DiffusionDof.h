#include "shader.h"
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

float betaForward(Texture2D<float> depthTex, int2 size, int2 xy, int2 forwardXyDelta, 
				  float2 projConstants, float focalPlane, int iterations)
{
	int2 xyNext = xy + forwardXyDelta;
	if((xyNext.x > size.x - 1) || (xyNext.y > size.y - 1)) return 0;
	float cocNext = z2coc(unproject_z(depthTex[xyNext], projConstants), 100, .5, focalPlane);
	float cocCurrent = z2coc(unproject_z(depthTex[xy], projConstants), 100, .5, focalPlane);
	float betaNext = beta(cocNext, iterations);
	float betaCurrent = beta(cocCurrent, iterations);
	return min(betaNext, betaCurrent);
}
ABCDTriple computeABCD(Texture2D<float> depthTex, Texture2D<float3> colorTex, int2 xy, int2 xyDelta, int2 size,				  
				  float2 projConstants, float focalPlane, int iterations)
{
	ABCDTriple abcd;
	//TODO: do boundary testing
	float betaA = betaForward(depthTex, size, xy - 2 * xyDelta, xyDelta, projConstants, focalPlane, iterations);
	//n-1 <-> n
	float betaB = betaForward(depthTex, size, xy - xyDelta, xyDelta, projConstants, focalPlane, iterations);
	//n <-> n+1
	float betaC = betaForward(depthTex, size, xy, xyDelta, projConstants, focalPlane, iterations);
	//n+1 <-> n+2
	float betaD = betaForward(depthTex, size, xy + xyDelta, xyDelta, projConstants, focalPlane, iterations);
	
	abcd.a = float3(-betaA, -betaB, -betaC);
	abcd.b = float3(1 + betaA + betaB, 1 + betaB + betaC, 1 + betaC + betaD);
	abcd.c = float3(-betaB, -betaC, -betaD);
	abcd.d = float3x3(colorTex[xy - xyDelta], colorTex[xy], colorTex[xy + xyDelta]);

	return abcd;
}