#include "stdafx.h"
#include "window.h"
#include <AntTweakBar.h>
#include <map>

std::map<Handle, Window*> active_windows;

SIZE Window::size() const
{		
	RECT rc;
	GetClientRect(handle, &rc);
	SIZE size;
	size.cx = rc.right - rc.left;
	size.cy = rc.bottom - rc.top;
	return size;
}


LRESULT CALLBACK MessageProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Send event message to AntTweakBar
	if (TwEventWin(wnd, message, wParam, lParam))
		return 0; // Event has been handled by AntTweakBar

	switch (message) 
	{
		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				BeginPaint(wnd, &ps);
				EndPaint(wnd, &ps);
				return 0;
			}
		case WM_SIZE: // Window size has been changed
			active_windows[wnd]->call_resize_callback();
			return 0;
		case WM_CHAR:
			if (wParam == VK_ESCAPE)
				PostQuitMessage(0);
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		default:
			return DefWindowProc(wnd, message, wParam, lParam);
	}
}

void Window::init(HINSTANCE instance)
{	
	WNDCLASSEX wcex = { sizeof(WNDCLASSEX), CS_HREDRAW|CS_VREDRAW, MessageProc,
						0L, 0L, instance, NULL, NULL, NULL, NULL, L"GFX", NULL };
	RegisterClassEx(&wcex);
	RECT rc = { 0, 0, 800, 600 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	handle = CreateWindow(L"gfx", L"GFX", 
							WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 
							rc.right-rc.left, rc.bottom-rc.top, NULL, NULL, instance, NULL);
	ShowWindow(handle, SW_SHOW);
	UpdateWindow(handle);	

	active_windows[handle] = this;
}
	
void Window::call_resize_callback()
{
	if(resize_callback)	resize_callback(this);
}