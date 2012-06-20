#pragma once
#include "Camera.h"
#include "window.h"
#include "d3d.h"
#include "gfx.h"
#include "package.h"
#include <vector>
#include "fx.h"
#include "fbx.h"
#include "Renderer.h"
#include <memory>

using namespace gfx;
using namespace std;
struct Core
{
	Core() { }
	void initialize(HINSTANCE instance);
	void load_models();
	void frame();
	


	Camera camera;

	vector<unique_ptr<RuntimeMesh>> meshes;

	package::Package dof_package;


	float light_dir_ws[3];

	Window window;
	D3D d3d;
	double frame_time;

	Renderer renderer;
};