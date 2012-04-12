#pragma once

#include <vector>
#include <memory>

#include "gfx.h"
#include <glm/glm.hpp>

using namespace glm;
namespace asset
{	
	namespace fbx
	{	
		struct Camera
		{
			vec3 eye;
			vec3 focus;
			vec3 up;
			float n; float f;
			float aspect_ratio;
			float fov;

		};
		shared_ptr<gfx::Model> load_animated_fbx_model(const char * path, vector<Camera>* cameras);
	}
}
