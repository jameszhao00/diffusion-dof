#include "stdafx.h"
#include "gfxdemo.h"
#include "fbx.h"
#include "package.h"
#include <iostream>
using namespace std;

//using namespace DirectX;

void resize(ID3D11Texture2D** texture,
			ID3D11Device* device,
			int w, int h)
{	
	
	string name = d3d::name(*texture);
	D3D11_TEXTURE2D_DESC tex2d_desc;
	(*texture)->GetDesc(&tex2d_desc);
	(*texture)->Release();
	
	
	tex2d_desc.Width = w;
	tex2d_desc.Height = h;
	device->CreateTexture2D(&tex2d_desc, nullptr, texture);
	d3d::name(*texture, name);	
}

void resize(ID3D11RenderTargetView** rtv, 
			ID3D11ShaderResourceView** srv,	
			ID3D11Texture2D** texture,
			ID3D11Device* device,
			int w, int h)
{	
	//the release()s call procedure is not working
	assert((rtv != nullptr) && (srv != nullptr));
	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
	
	string rtv_name = d3d::name(*rtv);
	string srv_name = d3d::name(*srv);

		int r = getref(device);

	(*rtv)->GetDesc(&rtv_desc);
	(*rtv)->Release();
	(*rtv) = nullptr;
	
	int	r2  = getref(device);
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		
	(*srv)->GetDesc(&srv_desc);
	(*srv)->Release();
	(*srv) = nullptr;
	int	r3  = getref(device);

	resize(texture, device, w, h);

	device->CreateRenderTargetView(*texture, &rtv_desc, rtv);
	device->CreateShaderResourceView(*texture, &srv_desc, srv);
	
	int	r4  = getref(device);
	d3d::name(*rtv, rtv_name);
	d3d::name(*srv, srv_name);

	int	r5  = getref(device);
}
void GfxDemo::init_gbuffers(int w, int h)
{	
	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = w;
	desc.Height = h;
	desc.ArraySize = 1;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;	
	desc.SampleDesc.Count = MSAA_COUNT;
	desc.SampleDesc.Quality = 0;
	desc.MipLevels = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;

	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	d3d.device->CreateTexture2D(&desc, nullptr, &normal);
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d3d.device->CreateTexture2D(&desc, nullptr, &albedo);
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	d3d.device->CreateTexture2D(&desc, nullptr, &debug[0]);
	desc.SampleDesc.Count = 1;
	d3d.device->CreateTexture2D(&desc, nullptr, &debug[1]);

	desc.MipLevels = 0;
	desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	d3d.device->CreateTexture2D(&desc, nullptr, &debug[2]);
	//DEBUG[1, 2] is NOT MSAA
	//debug 0 is msaa 
	//debug 1 is no msaa
	//debug 2 is no msaa and supports mip maps
	

	d3d::name(normal.p, "normal");
	d3d::name(albedo.p, "albedo");
	d3d::name(debug[0].p, "debug 0");
	d3d::name(debug[1].p, "debug 1");
	d3d::name(debug[2].p, "debug 2");

	d3d.device->CreateRenderTargetView(normal, nullptr, &normal_rtv);
	d3d.device->CreateRenderTargetView(albedo, nullptr, &albedo_rtv);
	d3d.device->CreateRenderTargetView(debug[0], nullptr, &debug_rtv[0]);
	d3d.device->CreateRenderTargetView(debug[1], nullptr, &debug_rtv[1]);
	d3d.device->CreateRenderTargetView(debug[2], nullptr, &debug_rtv[2]);

	d3d.device->CreateShaderResourceView(normal, nullptr, &normal_srv);
	d3d.device->CreateShaderResourceView(albedo, nullptr, &albedo_srv);
	d3d.device->CreateShaderResourceView(debug[0], nullptr, &debug_srv[0]);
	d3d.device->CreateShaderResourceView(debug[1], nullptr, &debug_srv[1]);
	//debug 2 needs mip support
	CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MostDetailedMip = 0;
	srv_desc.Texture2D.MipLevels = -1;
	srv_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	
	d3d.device->CreateShaderResourceView(debug[2], &srv_desc, &debug_srv[2]);
	
	d3d::name(normal_rtv.p, "normal");
	d3d::name(albedo_rtv.p, "albedo");
	d3d::name(debug_rtv[0].p, "debug");
	d3d::name(debug_rtv[1].p, "debug");
	d3d::name(debug_rtv[2].p, "debug");
	d3d::name(normal_srv.p, "normal");
	d3d::name(albedo_srv.p, "albedo");
	d3d::name(debug_srv[0].p, "debug");
	d3d::name(debug_srv[1].p, "debug");
	d3d::name(debug_srv[2].p, "debug");
}
void TW_CALL CopyCDStringToClient(char **destPtr, const char *src)
{
	size_t srcLen = (src!=NULL) ? strlen(src) : 0;
	size_t destLen = (*destPtr!=NULL) ? strlen(*destPtr) : 0;

	// alloc or realloc dest memory block if needed
	if( *destPtr==NULL )
		*destPtr = (char *)malloc(srcLen+1);
	else if( srcLen>destLen )
		*destPtr = (char *)realloc(*destPtr, srcLen+1);

	// copy src
	if( srcLen>0 )
		strncpy(*destPtr, src, srcLen);
	(*destPtr)[srcLen] = '\0'; // null-terminated string
}

#include <iostream>
#include <fstream>
using namespace std;
void TW_CALL save_img_callback(void *ptr)
{ 
	GfxDemo* demo = (GfxDemo*)ptr;

	std::string s(demo->save_name);
	std::wstring ws;
	ws.assign(s.begin(), s.end());
	{
		ID3D11Resource* backbuffer_tex;
		demo->d3d.back_buffer_rtv->GetResource(&backbuffer_tex);
		D3DX11SaveTextureToFile(demo->d3d.immediate_ctx, backbuffer_tex, D3DX11_IFF_BMP, (L"comparison/" + ws + L".bmp").c_str());
		backbuffer_tex->Release();
	}
	if(0)
	{
		//depth
		auto path = (L"comparison/" + ws + L"_depth.depth");
		ID3D11Texture2D* depth_tex;
		demo->d3d.depth_srv->GetResource((ID3D11Resource**)&depth_tex);


		D3D11_TEXTURE2D_DESC tex2d_desc;
		depth_tex->GetDesc(&tex2d_desc);
		ID3D11Texture2D* temp;
		tex2d_desc.BindFlags = 0;
		tex2d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ ;
		tex2d_desc.Usage = D3D11_USAGE_STAGING;
		demo->d3d.device->CreateTexture2D(&tex2d_desc, nullptr, &temp);
		demo->d3d.immediate_ctx->CopyResource(temp, depth_tex);

		D3D11_MAPPED_SUBRESOURCE mapped;
		demo->d3d.immediate_ctx->Map(temp, 0, D3D11_MAP_READ, 0, &mapped);

		ofstream output(path, std::ios::out | ios::binary | ios::trunc);
		for(int r = 0; r < tex2d_desc.Height; r++)
		{
			for(int c = 0; c < tex2d_desc.Width; c++)
			{
				float val = *(float*)((char*)mapped.pData + r * mapped.RowPitch + c * sizeof(float));
				
				output << val  << ",";				
			}
			output << endl;
		}
		assert(output.bad() == false);
		output.close();
		demo->d3d.immediate_ctx->Unmap(temp, 0);

		temp->Release();
		depth_tex->Release();
	}
}
void GfxDemo::init(HINSTANCE instance)
{
	//weird... why do we need to initalize?
	//if we don't init drawop's ccomptrs are initialized to garbage
	invert_depth = true;
	gbuffer_debug_mode = 0;
	aa_visualize = false;
	anim_frame = 0;
	blur_sigma = 3;

	XMStoreFloat4((XMFLOAT4*)obj_ori, XMQuaternionIdentity());
	window.init(instance);
	d3d.init(window);
	use_fresnel = true;
	

	load_shaders();
	load_models();
	init_gbuffers(window.size().cx, window.size().cy);
	
	is_top=false;
    TwBar *bar = TwNewBar("Scene");

	//obj_ori[0] = 0; obj_ori[1] = .93; obj_ori[2] = .37; obj_ori[3] = -0.07;
	//obj_ori[0] = 0; obj_ori[1] = 0; obj_ori[2] = 0; obj_ori[3] = 0;

	XMStoreFloat4((XMFLOAT4*)obj_ori, XMQuaternionIdentity());
	obj_ori[0] = 0.17; obj_ori[1] = 0.01; obj_ori[2] = -0; obj_ori[3] = .99;
	cam_dist = 0.57;
	
	light_dir_ws[0] = 0.24;
	light_dir_ws[1] = 0.55;
	light_dir_ws[2] = -0.80;
	do_anim = false;
	TwCopyCDStringToClientFunc(CopyCDStringToClient); // CopyCDStringToClient implementation is given above
	TwAddVarRW(bar, "Image Name", TW_TYPE_CDSTRING, &save_name, "");
	TwAddButton(bar, "Save Image", save_img_callback, this, "");
	save_name = strdup("");
	

	camera_i = 0;
	noise_ratio = 0.01;
	ssr_blur_ratio = 2.8;
	bilateral_z = true;
	TwAddVarRW(bar, "Camera", TW_TYPE_INT32, &camera_i, "min=0");
	TwAddVarRW(bar, "Orientation", TW_TYPE_QUAT4F, obj_ori, "opened=true axisy=y axisz=-z");
	TwAddVarRW(bar, "Dist", TW_TYPE_FLOAT, &cam_dist, "step=0.001");
	TwAddVarRW(bar, "Anim", TW_TYPE_BOOLCPP, &do_anim, "");
	TwAddVarRW(bar, "Is Top", TW_TYPE_BOOLCPP, &is_top, "");
	TwAddVarRW(bar, "Frame", TW_TYPE_UINT32, &anim_frame, "");
	TwAddVarRW(bar, "GBuffer Debug", TW_TYPE_UINT32, &gbuffer_debug_mode, "min=0 max=3");
	TwAddVarRW(bar, "Blur Sigma", TW_TYPE_FLOAT, &blur_sigma, "min=1 max=9 step=0.05");
	TwAddVarRW(bar, "Noise Ratio", TW_TYPE_FLOAT, &noise_ratio, "min=0.001 max=0.2 step=0.001");
	TwAddVarRW(bar, "SSR Blur Ratio", TW_TYPE_FLOAT, &ssr_blur_ratio, "min=0 max=35 step=0.05");
	TwAddVarRW(bar, "Bilateral Z", TW_TYPE_BOOLCPP, &bilateral_z, "");

	TwAddVarRW(bar, "DX", TW_TYPE_FLOAT, &dx, "step=0.0005");
	TwAddVarRW(bar, "Viz X", TW_TYPE_INT32, &vx, "step=1");
	TwAddVarRW(bar, "Viz Y", TW_TYPE_INT32, &vy, "step=1");
	vx = 300; vy = 300;


	
	TwAddVarRW(bar, "Use Fresnel", TW_TYPE_BOOLCPP, &use_fresnel, "");
	TwAddVarRW(bar, "Light Dir", TW_TYPE_DIR3F, light_dir_ws, "opened=true axisy=-y axisx=-x");

	
	

	D3D11_DEPTH_STENCIL_DESC ds_desc;
	ZeroMemory(&ds_desc, sizeof(ds_desc));
	ds_desc.DepthFunc = D3D11_COMPARISON_GREATER;
	ds_desc.DepthEnable = true;
	ds_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	d3d.immediate_ctx->OMGetDepthStencilState(&ds_state, nullptr);
	d3d.device->CreateDepthStencilState(&ds_desc, &inverted_ds_state);
	d3d::name(inverted_ds_state.p, "inverted ds");
    

	gfx_profiler.init(&d3d);
}
#include <iostream>
float dt = 0;
int debug_render_frame = 0;
template<typename T>
void clear_views(T** views, int num_views)
{
	for(int i = 0; i < num_views; i++) views[i] = nullptr;
}
void GfxDemo::frame()
{		

	if(d3d.device == nullptr) return;

	D3D11_TEXTURE2D_DESC t2d_desc;
	ID3D11Texture2D* normal_resource = nullptr;
	normal_srv->GetResource((ID3D11Resource**)&normal_resource);
	normal_resource->GetDesc(&t2d_desc);
	normal_resource->Release();

	gfx_profiler.begin_frame();

	if((window.size().cx != t2d_desc.Width) || (window.size().cy != t2d_desc.Height))
	{
		//recreate everything
		resize(&normal_rtv.p, &normal_srv.p, &normal.p, d3d.device, window.size().cx, window.size().cy);		
		resize(&albedo_rtv.p, &albedo_srv.p, &albedo.p, d3d.device, window.size().cx, window.size().cy);
		resize(&debug_rtv[0].p, &(debug_srv[0].p), &debug[0].p, d3d.device, window.size().cx, window.size().cy);
		resize(&debug_rtv[1].p, &(debug_srv[1].p), &debug[1].p, d3d.device, window.size().cx, window.size().cy);
		resize(&debug_rtv[2].p, &(debug_srv[2].p), &debug[2].p, d3d.device, window.size().cx, window.size().cy);
		
	}
	gpu_env.vp_w = window.size().cx;
	gpu_env.vp_h = window.size().cy;
	dt += 0.001f;
	float color[4] = {0, 0, 0.0f, 0};
	float neg_one[4] = {-1, -1, -1, -1};
	float one[4] = {1, 1, 1, 1};
	d3d.immediate_ctx->ClearRenderTargetView(d3d.back_buffer_rtv, color);
	d3d.immediate_ctx->ClearRenderTargetView(debug_rtv[0], color);
	d3d.immediate_ctx->ClearRenderTargetView(debug_rtv[1], color);
	d3d.immediate_ctx->ClearRenderTargetView(debug_rtv[2], color);
	d3d.immediate_ctx->ClearRenderTargetView(normal_rtv, color);
	d3d.immediate_ctx->ClearRenderTargetView(albedo_rtv, one);
	
	if(invert_depth)
	{
		d3d.immediate_ctx->ClearDepthStencilView(d3d.dsv, D3D11_CLEAR_DEPTH, 0, 0);
		
		d3d.immediate_ctx->OMSetDepthStencilState(inverted_ds_state, 0);
	}
	else
	{
		d3d.immediate_ctx->ClearDepthStencilView(d3d.dsv, D3D11_CLEAR_DEPTH, 1, 0);
		d3d.immediate_ctx->OMSetDepthStencilState(ds_state, 0);
	}

	d3d.immediate_ctx->VSSetShader(vs, nullptr, 0);
	d3d.immediate_ctx->PSSetShader(ps, nullptr, 0);

	d3d::cbuffers::ObjectCB object_cb_data;
	d3d::cbuffers::ObjectAnimationCB object_animation_cb_data;
	d3d::cbuffers::ShadeGBufferDebugCB gbuffer_debug_cb_data;
	d3d::cbuffers::FSQuadCb fsquad_cb_data;

	gbuffer_debug_cb_data.render_mode = gbuffer_debug_mode;
	gbuffer_debug_cb_data.use_fresnel = use_fresnel ? 1 : 0;
	gbuffer_debug_cb_data.light_dir_ws[0] = 500 * light_dir_ws[0];
	gbuffer_debug_cb_data.light_dir_ws[1] = 500 * light_dir_ws[1];
	gbuffer_debug_cb_data.light_dir_ws[2] = 500 * light_dir_ws[2];
	gbuffer_debug_cb_data.light_dir_ws[3] = 1;
	/*
	auto joint_n = package.skeletal_animations[0].joints_count;
	auto frame_n = package.skeletal_animations[0].frames_count;
	int frame = anim_frame % frame_n;
	auto bone_data = &package.skeletal_animations[0].data[frame * joint_n];
	
	memcpy(&object_animation_cb_data, bone_data, sizeof(package::m44) * joint_n);
	*/
	
	SIZE window_size = window.size();

	auto w = XMMatrixRotationQuaternion(XMLoadFloat4((XMFLOAT4*)obj_ori));
	cam_focus[0] = dx;

	//auto v = XMMatrixLookAtLH(XMVectorSet(cam_focus[0], cam_focus[1], cam_focus[2] - cam_dist, 1), XMLoadFloat4((XMFLOAT4*)cam_focus), XMVectorSet(0, 1, 0, 0));
	XMMATRIX v;
	if(is_top)
	{
		v = XMMatrixLookAtLH(XMVectorSet(0, 80, 0, 1), XMVectorSet(0, 0, 0, 1), XMVectorSet(0, 0, 1, 0));
	}
	else
	{
		v = XMMatrixLookAtLH(XMVectorSet(0, 50 * cam_dist, -90 * cam_dist, 1), XMVectorSet(0, 0, 0, 1), XMVectorSet(0, 1, 0, 0));
	}

	auto current_cam = cameras[camera_i % cameras.size()];

	////////////////////CAMERA
	if(0) {
	v=   XMMatrixLookAtLH(
		XMVectorSet(current_cam.eye.x, current_cam.eye.y, current_cam.eye.z, 1), 
		XMVectorSet(current_cam.focus.x, current_cam.focus.y, current_cam.focus.z, 1), 
		XMVectorSet(current_cam.up.x, current_cam.up.y, current_cam.up.z, 0)
		); }
	///////////////////////

	float n = 1; float f = 300; 

	if(invert_depth)
	{
		n = 300; f = 1; 
	}
	float fov = XM_PI / 4.0f;
	float ar = window_size.cx / (float)window_size.cy;
	////////////////////CAMERA
	if(0)
	{
		n = current_cam.f; f = current_cam.n;
		ar = current_cam.aspect_ratio; fov = current_cam.fov;
	}
	///////////////////////
	auto p = XMMatrixPerspectiveFovLH(fov, ar, n, f);
	//0 = f / (f - n)
	//1 = (-f * n) / (f - n)
	//near, far
	fsquad_cb_data.proj_constants[0] = f / (f - n);
	fsquad_cb_data.proj_constants[1] = (-f * n) / (f - n);
	fsquad_cb_data.debug_vars[0] = cos(fov * 0.5 )/sin(fov * 0.5 );
	fsquad_cb_data.debug_vars[1] = vx;
	//fsquad_cb_data.debug_vars[2] = vy;
	fsquad_cb_data.debug_vars[2] = ssr_blur_ratio;
	fsquad_cb_data.debug_vars[3] = noise_ratio;
	fsquad_cb_data.vars[0] = (float)bilateral_z;
	
	auto wv = XMMatrixMultiply(w, v);
	auto wvp = XMMatrixMultiply(wv, p);
	XMVECTOR v_det;
	XMStoreFloat4x4((XMFLOAT4X4*)&fsquad_cb_data.inv_p, XMMatrixTranspose(XMMatrixInverse(&v_det, p)));
	XMStoreFloat4x4((XMFLOAT4X4*)&fsquad_cb_data.proj, XMMatrixTranspose(p));
	XMStoreFloat4x4((XMFLOAT4X4*)&object_cb_data.wv, XMMatrixTranspose(wv));
	XMStoreFloat4x4((XMFLOAT4X4*)&object_cb_data.wvp, XMMatrixTranspose(wvp));
	XMStoreFloat4x4((XMFLOAT4X4*)&gbuffer_debug_cb_data.view, XMMatrixTranspose(v));

	d3d.sync_to_cbuffer(object_animation_cb, object_animation_cb_data);
	fx::update_gpu_env(&d3d, &fsquad_cb_data, &gpu_env);
	
	d3d.immediate_ctx->VSSetConstantBuffers(0, 1, &object_cb.p);
	d3d.immediate_ctx->VSSetConstantBuffers(1, 1, &object_animation_cb.p);
	d3d.immediate_ctx->PSSetConstantBuffers(0, 1, &object_cb.p);
	
	unsigned int offset = 0;
	d3d.immediate_ctx->IASetInputLayout(drawop.il);
	d3d.immediate_ctx->IASetVertexBuffers(0, 1, &drawop.vb.p, &drawop.vb_stride, &offset);
	d3d.immediate_ctx->IASetIndexBuffer(drawop.ib, DXGI_FORMAT_R32_UINT, 0);
	d3d.immediate_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const int srvs_count = 7;
	const int rtvs_count = 6;

	ID3D11ShaderResourceView* srvs[srvs_count] = {
		nullptr, 
		nullptr, 
		nullptr, 
		nullptr,
		nullptr,
		nullptr,
		nullptr
	};
	
	d3d.immediate_ctx->PSSetShaderResources(0, srvs_count, srvs);
	ID3D11RenderTargetView* rtvs[rtvs_count] = {
		normal_rtv, 
		albedo_rtv, 
		debug_rtv[0], 
		nullptr,
		nullptr,
		nullptr,
	};
	d3d.immediate_ctx->OMSetRenderTargets(rtvs_count, rtvs, d3d.dsv);
	
	for(auto i = 0; i < package.meshes[0].mesh_parts.size(); i++)
	{
		int count =  package.meshes[0].mesh_parts.size();
		auto mesh_part = package.meshes[0].mesh_parts[i];
		
		//we need to sync base_primitive id for each mesh part
		//object_cb_data.base_primitive_id[0] = mesh_part.indices_offset / 3;
		object_cb_data.misc[0] = (i == 0) ? 1 : 0;
		d3d.sync_to_cbuffer(object_cb, object_cb_data);
		if(!package.materials[mesh_part.material].textures.empty())
		{
			auto texture = textures.at(package.materials[mesh_part.material].textures.front());
			srvs[0] = texture.p;
		}
		else
		{
			srvs[0] = nullptr;
		}
		d3d.immediate_ctx->PSSetSamplers(0, 1, &gpu_env.linear_sampler.p);
		d3d.immediate_ctx->PSSetShaderResources(0, srvs_count, srvs);
		d3d.immediate_ctx->DrawIndexed(mesh_part.indices_count, mesh_part.indices_offset, 0);		
	}
	
	fx::shade_gbuffer(&d3d, &gpu_env, &shade_gbuffer_ctx, 
		albedo_srv, 
		normal_srv, 
		d3d.depth_srv, 
		&gbuffer_debug_cb_data, 
		debug_rtv[0]);		

	//d3d.immediate_ctx->GenerateMips(debug_srv[2]);

	d3d.immediate_ctx->PSSetSamplers(0, 1, &gpu_env.linear_sampler.p);
	d3d.immediate_ctx->PSSetSamplers(1, 1, &gpu_env.aniso_sampler.p);
	//fx::blur(&d3d, &gpu_env, &fx_env.blur_ctx, fx::eHorizontal, 5, debug_srv[0], debug_rtv[1]);
	//fx::blur(&d3d, &gpu_env, &fx_env.blur_ctx, fx::eVertical, 5, debug_srv[1], debug_rtv[2]);


	gfx_profiler.begin_block("ssr");

	fx::ssr(&d3d, &gpu_env, &ssr_ctx, normal_srv, debug_srv[0], d3d.depth_srv, noise, 
		debug_srv[1], debug_srv[2], debug_rtv[1], debug_rtv[2],
		d3d.back_buffer_rtv);
	gfx_profiler.end_block();
	
	/*
	fx::luminance(&d3d, &gpu_env, &fx_env.luminance_ctx, debug_srv[0],  debug_rtv[2]);
	//d[2] is now lum
	d3d.immediate_ctx->GenerateMips(debug_srv[2]);
	//d[2] mips now have avg lum	
	fx::lum_highpass(&d3d, &gpu_env, &fx_env.lum_highpass_ctx, 2,
		debug_srv[0], debug_rtv[1]);		
	//d[1] now has colors above lum
	fx::blur(&d3d, &gpu_env, &fx_env.blur_ctx,
		fx::eHorizontal, blur_sigma, debug_srv[1], debug_rtv[2]);
	//d[2] now has h blurred bloom
	fx::blur(&d3d, &gpu_env, &fx_env.blur_ctx,
		fx::eVertical, blur_sigma, debug_srv[2], debug_rtv[1]);
	//d[1] now has v blurred bloom
	//d3d.immediate_ctx->ResolveSubresource(debug[2], 0, debug[0], 0, 
	//	DXGI_FORMAT_R16G16B16A16_FLOAT);
	//d[2] now has color
	float dummy[4];
	//d3d.immediate_ctx->OMSetBlendState(gpu_env.additive_blend, dummy, 1);
	fx::tonemap(&d3d, &gpu_env, &tonemap_ctx, debug_srv[0], d3d.back_buffer_rtv);
	//d3d.immediate_ctx->OMSetBlendState(gpu_env.standard_blend, dummy, 1);
	//resort to extra add b/c blend just doesn't seem to work
	//fx::additive_blend(&d3d, &gpu_env, &fx_env.additive_blend_ctx, 
	//	debug_srv[2], debug_srv[1], d3d.back_buffer_rtv);
	*/
	if((debug_render_frame % 3 == 0) && do_anim) anim_frame++;
	
	//for debugging purposes
	if(debug_render_frame < 2)
	{
		ID3D11Resource* backbuffer_tex;
		d3d.back_buffer_rtv->GetResource(&backbuffer_tex);
		if(debug_render_frame == 0)	
		{
			D3DX11SaveTextureToFile(d3d.immediate_ctx, backbuffer_tex, D3DX11_IFF_BMP, L"comparison/front.bmp");
			is_top = true;
		}
		else if(debug_render_frame == 1)
		{
			D3DX11SaveTextureToFile(d3d.immediate_ctx, backbuffer_tex, D3DX11_IFF_BMP, L"comparison/top.bmp");
			is_top=false;
		}
		backbuffer_tex->Release();
	}
	debug_render_frame++;
	d3d.swap_buffers();	

	gfx_profiler.end_frame();

	std::cout<<"ssr time = " << gfx_profiler.blocks["ssr"].ms << endl;

}

void GfxDemo::load_models()
{
	if(1)
	{		
		model = asset::fbx::load_animated_fbx_model("assets/source/ssr/ref3.fbx", &cameras);
		//model = asset::fbx::load_animated_fbx_model("assets/source/cb.fbx");
	
	
		list<const Model*> models;
		models.push_back(model.get());
		package::bake_package(L"assets/ssr3.package", &models);
	}
	
	cam_focus[0] = 0;//model->center[0];
	dx = 0;
	cam_focus[1] = 40;// model->center[1];
	cam_focus[2] = 0;//model->center[2];

	package::load_package(L"assets/ssr3.package", &package);

	d3d.create_draw_op(
		package.meshes[0].verticies, 
		package.meshes[0].vertex_count, 
		package.meshes[0].vertex_stride, 
		(unsigned int*)package.meshes[0].indices, 
		package.meshes[0].indices_count, 
		il, &drawop);

	for(int i = 0; i < package.textures.size(); i++)
	{
		CComPtr<ID3D11ShaderResourceView> srv;
		auto hr = D3DX11CreateShaderResourceViewFromMemory(
			d3d.device, 
			package.textures.at(i).data, 
			package.textures.at(0).byte_size, 
			nullptr,
			nullptr, 
			&srv.p,
			nullptr);
		textures.push_back(srv);
	}
	D3DX11_IMAGE_LOAD_INFO load_info;
	load_info.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	auto hr = D3DX11CreateShaderResourceViewFromFile(d3d.device, L"assets/source/ss_noise.png", &load_info, NULL, &noise, nullptr);
	
}
void GfxDemo::load_shaders()
{
	//assert(0);// output linear Z so we don't have to convert from ndcz to linear z every pixel in ssr
	d3d.create_shaders_and_il(L"shaders/standard_animated.hlsl", &vs.p, &ps.p, nullptr, &il.p, 
		gfx::VertexTypes::eAnimatedStandard);
	object_cb = d3d.create_cbuffer<d3d::cbuffers::ObjectCB>();
	object_animation_cb = d3d.create_cbuffer<d3d::cbuffers::ObjectAnimationCB>();

	fx::make_shade_gbuffer_ctx(&d3d, &shade_gbuffer_ctx);
	fx::make_gpu_env(&d3d, &gpu_env);
	fx::make_fx_env(&d3d, &fx_env);
	fx::make_tonemap_ctx(&d3d, &tonemap_ctx);
	fx::make_ssr_ctx(&d3d, &ssr_ctx);
}