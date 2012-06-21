#include "stdafx.h"
#include "gfxdemo.h"
int main()
{	
	/*
	uint r = 0;
	uint r2 = 0;
	uint r3 = 0;
	{	
		//pack
		float k = 5;
		uint ki = *(uint*)&k;
		float k2 = 6;
		uint k2i = *(uint*)&k2;

		r = (ki & 0xffffffC0) | (k2i & 0x3F);
		r2 = (k2i)
	}
	{
		uint rup = (r & 0xffffffc0);
		uint rup2 = (r & 0x3f);
		//unpack
		float k = *(float*)(&rup);
		float k2 = *(float*)(&rup2);
	}
	*/
	Core demo;	
	demo.initialize(GetModuleHandle(nullptr));
	while (!demo.window.quitFlag)
	{
		demo.frame();
	}
	demo.d3d.immediate_ctx->ClearState();
	demo.d3d.immediate_ctx->Flush();
	//HACK: move this elsewhere
	TwTerminate();
}
