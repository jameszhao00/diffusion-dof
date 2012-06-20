#include "stdafx.h"
#include <iostream>
#include <sstream>
#include <fstream>

#include "gfxdemo.h"
#include "fbx.h"
#include "package.h"
#include "ui.h"
#include "Camera.h"
using namespace std;
void Core::initialize(HINSTANCE instance)
{
	window.initialize(instance);
	d3d.init(window);	
	renderer.initialize(d3d);

	load_models();
    TwBar *bar = TwNewBar("Scene");

	light_dir_ws[0] = 0.24;
	light_dir_ws[1] = 0.55;
	light_dir_ws[2] = -0.80;

	TwAddVarRW(bar, "Light Dir", TW_TYPE_DIR3F, light_dir_ws, "opened=true axisy=-y axisz=-z");
}
#include <iostream>
float dt = 0;

void Core::frame()
{		
	window.handleEvents();
	if(d3d.device == nullptr) return;
	
	renderer.beginFrame(int2(window.size().cx, window.size().cy));
		
	for(const auto& mesh : meshes)
	{
		if(mesh->type == RuntimeMesh::eAnimated)
		{
			//renderer.drawAnimated(*mesh, float4x4(), (float4x4*)dof_package.skeletal_animations[0].data, 
			//				dof_package.skeletal_animations[0].joints_count);
			renderer.drawAnimated(*mesh, float4x4(), nullptr, 
								0);
		}
		else if(mesh->type == RuntimeMesh::eStatic)
		{

		}
		
	}
	
	renderer.deferredPass(float3(light_dir_ws[0], light_dir_ws[1], light_dir_ws[2]) * 100.f);
	renderer.postFxPass();
	renderer.endFrame();
}

void Core::load_models()
{
	auto model = asset::fbx::load_animated_fbx_model("assets/source/dof_test1.fbx", nullptr);
	//auto dude_model = asset::fbx::load_animated_fbx_model("assets/source/dude.fbx", &cameras);
	
	
	list<const Model*> models;
	models.push_back(model.get());
	package::bake_package(L"assets/dof_test1.package", &models);
	//models.clear();
	//models.push_back(dude_model.get());
	//package::bake_package(L"assets/dude.package", &models);
	

	//package::load_package(L"assets/dof_test1.package", &package);
	
	//package::load_package(L"assets/dude.package", &dof_package);
	package::load_package(L"assets/dof_test1.package", &dof_package);

	d3d::DrawOp drawop_dof;
	d3d.create_draw_op(
		dof_package.meshes[0].verticies, 
		dof_package.meshes[0].vertex_count, 
		dof_package.meshes[0].vertex_stride, 
		(unsigned int*)dof_package.meshes[0].indices, 
		dof_package.meshes[0].indices_count, &drawop_dof);

	unique_ptr<RuntimeMesh> dude_rm(new RuntimeMesh());
	dude_rm->type = RuntimeMesh::eAnimated;
	dude_rm->live.ib = drawop_dof.ib;
	dude_rm->live.vb = drawop_dof.vb;
	dude_rm->live.vertexStride = drawop_dof.vb_stride;
	for(int i = 0; i < dof_package.textures.size(); i++)
	{
		CComPtr<ID3D11ShaderResourceView> srv;
		auto hr = D3DX11CreateShaderResourceViewFromMemory(
			d3d.device, 
			dof_package.textures.at(i).data, 
			dof_package.textures.at(0).byte_size, 
			nullptr,
			nullptr, 
			&srv.p,
			nullptr);
		dude_rm->live.textures.push_back(srv);
	}
	for(auto & part : dof_package.meshes[0].mesh_parts)
	{
		RuntimeMeshPart meshPart;
		meshPart.indexCount = part.indices_count;
		meshPart.indexOffset = part.indices_offset;
		meshPart.textures.push_back(part.material);
		dude_rm->parts.push_back(meshPart);
	}
	meshes.push_back(std::move(dude_rm));
}