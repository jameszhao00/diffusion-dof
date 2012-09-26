#include "stdafx.h"
#include "GBuffer.h"

void GBuffer::initialize( D3D& pd3d )
{
	d3d = &pd3d;
	normal.configure("normal", DXGI_FORMAT_R16G16B16A16_FLOAT);
	albedo.configure("albedo", DXGI_FORMAT_R16G16B16A16_FLOAT);
	for(int i = 0; i < 3; i++) 
	{
		scratch[i].configure("debug " + i, DXGI_FORMAT_R16G16B16A16_FLOAT);
	}

	scratchABC.configure("ddof abc", DXGI_FORMAT_R32G32B32A32_UINT);

	scratchABCD[0].configure("ddof abcd 0", DXGI_FORMAT_R32G32B32A32_UINT);
	scratchABCD[1].configure("ddof abcd 1", DXGI_FORMAT_R32G32B32A32_UINT);
}

void GBuffer::resize( int w, int h )
{
	normal.initialize(*d3d, w, h);
	albedo.initialize(*d3d, w, h);
	for(int i = 0; i < 3; i++)
	{
		scratch[i].initialize(*d3d, w, h, true);
	}
	scratchABC.initialize(*d3d, w, h, true);
	scratchABCD[0].initialize(*d3d, w, h, true);
	scratchABCD[1].initialize(*d3d, w, h, true);
	ddofOutputStructureBuffer.initializeSB(*d3d, 3 * sizeof(float) * w * h, 3 * sizeof(float));
}

void GBuffer::clearAll()
{
	for(int i = 0; i < 3; i++) scratch[i].clear(d3d->immediate_ctx, 0, 0, 0, 0);	
	normal.clear(d3d->immediate_ctx, 0, 0, 0, 0);
	albedo.clear(d3d->immediate_ctx, 0, 0, 0, 0);
	scratchABC.clear(d3d->immediate_ctx, 0, 0, 0, 0);
	scratchABCD[0].clear(d3d->immediate_ctx, 0, 0, 0, 0);
	scratchABCD[1].clear(d3d->immediate_ctx, 0, 0, 0, 0);
}
