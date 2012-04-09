#pragma once

#include <vector>
#include <memory>

#include "gfx.h"

namespace asset
{	
	namespace fbx
	{	
		shared_ptr<gfx::Model> load_animated_fbx_model(const char * path);
	}
}
