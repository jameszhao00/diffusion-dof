#include "stdafx.h"
#include "gfxdemo.h"
int main()
{
	ID3D11Debug* debug;
	{
		GfxDemo demo;	
		demo.init(GetModuleHandle(nullptr));
		demo.d3d.device->QueryInterface(IID_ID3D11Debug, (void**)&debug);
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
		TwTerminate();
	}
	debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
	debug->Release();
}
