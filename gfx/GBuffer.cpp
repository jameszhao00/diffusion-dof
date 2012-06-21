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
	//scratchABC.configure("ddof abc", DXGI_FORMAT_R32G32B32A32_FLOAT);
	scratchD.configure("ddof d", DXGI_FORMAT_R16G16B16A16_FLOAT);
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
	scratchD.initialize(*d3d, w, h, true);
}

void GBuffer::clearAll()
{
	for(int i = 0; i < 3; i++) scratch[i].clear(d3d->immediate_ctx, 0, 0, 0, 0);	
	normal.clear(d3d->immediate_ctx, 0, 0, 0, 0);
	albedo.clear(d3d->immediate_ctx, 0, 0, 0, 0);
	scratchABC.clear(d3d->immediate_ctx, 0, 0, 0, 0);
	scratchD.clear(d3d->immediate_ctx, 0, 0, 0, 0);
}
