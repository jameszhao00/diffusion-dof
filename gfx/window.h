#pragma once
#include <functional>
#include "windows.h"
#include <memory>

typedef HWND Handle;
struct Window
{
	Window() : handle(nullptr), quitFlag(false) { }
	void initialize(HINSTANCE instance);
	SIZE size() const;
	void handleEvents();
	void handleResize();
	void handleQuit();
	Handle handle;
	std::function<void (const Window *)> resizeCallback;
	bool quitFlag;
};