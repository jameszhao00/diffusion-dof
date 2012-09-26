#include "stdafx.h"
#include "gfxdemo.h"
int main()
{	
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
