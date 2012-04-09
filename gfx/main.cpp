#include "stdafx.h"
#include "gfxdemo.h"
int main()
{
	GfxDemo demo;
	demo.init(GetModuleHandle(nullptr));
	
	MSG msg = {0};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
		else
		{
			demo.frame();
		}
    }
	demo.d3d.immediate_ctx->ClearState();
	demo.d3d.immediate_ctx->Flush();
	//TWFUNC TwTerminate();
}
