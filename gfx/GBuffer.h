#pragma once

#include "d3d.h"
struct GBuffer
{
	void clearAll();
	void initialize(D3D& pd3d);
	void resize(int w, int h);
	D3D* d3d;
	Texture2D albedo;
	Texture2D normal;
	Texture2D scratch[4];

	Texture2D scratchABC;
	Texture2D scratchABCD[2];

	Texture2D ddofOutputStructureBuffer;
};