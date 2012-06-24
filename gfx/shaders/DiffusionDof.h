
void ddofUnpack(uint4 input, out float a, out float b, out float c, out float3 d)
{
	//works
	d = float3(f16tof32(input.x), f16tof32(input.x >> 16), 
		f16tof32((input.y & 0x3f) | (input.y << 6 | 0x3f) | (input.y << 12 | 0x3f)));
	
	a = asfloat(input.y & 0xffffffc0);
	b = asfloat(input.z & 0xffffffc0);
	c = asfloat(input.w & 0xffffffc0);

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

int coordNOffset(int i, int passIdx)
{
	int k = pow(2, passIdx+1);
	return i * k + k - 2 + 1;
}
int2 calcXYDelta(int2 rawXyDelta, int passIdx)
{
	return rawXyDelta * pow(2, passIdx + 1);
}

struct ABCD
{
	float3 a;
	float3 b;
	float3 c;
	float3x3 d;
};
ABCD readABCD(Texture2D<uint4> abcdTex, int2 xy, int2 xyDelta)
{
	//out of bounds = read 0
	int2 xy0 = xy - xyDelta;
	int2 xy1 = xy;
	int2 xy2 = xy + xyDelta;
	ABCD abcd;
	ddofUnpack(abcdTex[xy0], abcd.a[0], abcd.b[0], abcd.c[0], abcd.d[0]);
	ddofUnpack(abcdTex[xy1], abcd.a[1], abcd.b[1], abcd.c[1], abcd.d[1]);
	ddofUnpack(abcdTex[xy2], abcd.a[2], abcd.b[2], abcd.c[2], abcd.d[2]);
	
	return abcd;
}