#pragma once
#include <functional>
#include "windows.h"
#include <memory>

typedef HWND Handle;
struct Window
{
	//resizeCounter starts at 1... because we need to handle the initial resize
	Window() : resizeCounter(1), handle(0) { }
	void init(HINSTANCE instance);
	SIZE size() const;
	void call_resize_callback();
	Handle handle;
	int resizeCounter;
	std::function<void (const Window *)> resize_callback;
};