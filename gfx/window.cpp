
#include "stdafx.h"
#include <AntTweakBar.h>
#include <map>
#include <sdl/SDL.h>
#include "window.h"

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
void Window::initialize(HINSTANCE instance)
{	

	SDL_Init(SDL_INIT_VIDEO);
	SDL_SetVideoMode(800, 600, SDL_GetVideoInfo()->vfmt->BitsPerPixel, SDL_RESIZABLE);
	handle = GetActiveWindow();
}
	
void Window::handleResize()
{
	if(resizeCallback)	resizeCallback(this);
}

void Window::handleEvents()
{
	SDL_Event evt;
	while(SDL_PollEvent(&evt))
	{
		if(!TwEventSDL(&evt, SDL_MAJOR_VERSION, SDL_MINOR_VERSION))
		{
			switch(evt.type)
			{
			case SDL_VIDEORESIZE:
				handleResize();
				break;
			case SDL_QUIT:
				handleQuit();
				break;
			case SDL_KEYDOWN:
				break;
			case SDL_KEYUP:
				break;
			}
		}		
	}

}

void Window::handleQuit()
{
	quitFlag = true;
}
