#pragma once
#include <functional>
#include "windows.h"
#include <memory>

typedef HWND Handle;
struct Window
{
	void init(HINSTANCE instance);
	SIZE size() const;
	void call_resize_callback();
	Handle handle;
	
	std::function<void (const Window *)> resize_callback;
};