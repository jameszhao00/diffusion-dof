#include "stdafx.h"
#include "gfxdemo.h"
#include "fbx.h"
#include "package.h"
#include <iostream>

using namespace std;

using namespace DirectX;

#define in
#define inout
#define out


void resize(ID3D11Texture2D** texture,
			int w, int h)
{	

	string name = d3d::name(*texture);
	D3D11_TEXTURE2D_DESC tex2d_desc;
	(*texture)->GetDesc(&tex2d_desc);	
	ID3D11Device* device;
	(*texture)->GetDevice(&device);
	(*texture)->Release();
	
	
	tex2d_desc.Width = w;
	tex2d_desc.Height = h;
	device->CreateTexture2D(&tex2d_desc, nullptr, texture);
	d3d::name(*texture, name);
	device->Release();
}

void resize(ID3D11RenderTargetView** rtv, 
			ID3D11ShaderResourceView** srv,			
			int w, int h,
			ID3D11Texture2D** original_tex = nullptr)
{	
	//the release()s call procedure is not working
	assert((rtv != nullptr) && (srv != nullptr));
	ID3D11Texture2D* texture;
	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
	
	string rtv_name = d3d::name(*rtv);
	string srv_name = d3d::name(*srv);

	ID3D11Device* device;
	(*rtv)->GetDevice(&device);

	(*rtv)->GetResource((ID3D11Resource**)&texture);
	(*rtv)->GetDesc(&rtv_desc);
	(*rtv)->Release();
	(*rtv) = nullptr;
	
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		
	//can't call again.. adding ref too many times...
	//(*srv)->GetResource((ID3D11Resource**)&texture);
	(*srv)->GetDesc(&srv_desc);
	(*srv)->Release();
	(*srv) = nullptr;
	if(original_tex != nullptr)
	{
		resize(original_tex, w, h);
		texture = *original_tex;
	}
	else
	{
		resize(&texture, w, h);
	}

	device->CreateRenderTargetView(texture, &rtv_desc, rtv);
	device->CreateShaderResourceView(texture, &srv_desc, srv);

	d3d::name(*rtv, rtv_name);
	d3d::name(*srv, srv_name);

	device->Release();
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
	ID3D11Texture2D* normal;
	ID3D11Texture2D* albedo;


	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	d3d.device->CreateTexture2D(&desc, nullptr, &normal);
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d3d.device->CreateTexture2D(&desc, nullptr, &albedo);
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	d3d.device->CreateTexture2D(&desc, nullptr, &debug[0]);
	desc.SampleDesc.Count = 1;
	d3d.device->CreateTexture2D(&desc, nullptr, &debug[1]);
	d3d.device->CreateTexture2D(&desc, nullptr, &debug[2]);
	//DEBUG[1, 2] is NOT MSAA
	

	d3d::name(normal, "normal");
	d3d::name(albedo, "albedo");
	d3d::name(debug[0], "debug 0");
	d3d::name(debug[1], "debug 1");
	d3d::name(debug[2], "debug 2");

	d3d.device->CreateRenderTargetView(normal, nullptr, &normal_rtv);
	d3d.device->CreateRenderTargetView(albedo, nullptr, &albedo_rtv);
	d3d.device->CreateRenderTargetView(debug[0], nullptr, &debug_rtv[0]);
	d3d.device->CreateRenderTargetView(debug[1], nullptr, &debug_rtv[1]);
	d3d.device->CreateRenderTargetView(debug[2], nullptr, &debug_rtv[2]);
	d3d.device->CreateShaderResourceView(normal, nullptr, &normal_srv);
	d3d.device->CreateShaderResourceView(albedo, nullptr, &albedo_srv);
	d3d.device->CreateShaderResourceView(debug[0], nullptr, &debug_srv[0]);
	d3d.device->CreateShaderResourceView(debug[1], nullptr, &debug_srv[1]);
	d3d.device->CreateShaderResourceView(debug[2], nullptr, &debug_srv[2]);
	
	d3d::name(normal_rtv, "normal");
	d3d::name(albedo_rtv, "albedo");
	d3d::name(debug_rtv[0], "debug");
	d3d::name(debug_rtv[1], "debug");
	d3d::name(debug_rtv[2], "debug");
	d3d::name(normal_srv, "normal");
	d3d::name(albedo_srv, "albedo");
	d3d::name(debug_srv[0], "debug");
	d3d::name(debug_srv[1], "debug");
	d3d::name(debug_srv[2], "debug");

	normal->Release();
	albedo->Release();
	//debug[0]->Release();
	//debug[1]->Release();
}

void GfxDemo::init(HINSTANCE instance)
{
	invert_depth = true;
	gbuffer_debug_mode = 0;
	d3d.device = nullptr;
	aa_visualize = false;
	anim_frame = 0;
	blur_sigma = 3;
	wireframe_mode = false;

	XMStoreFloat4((XMFLOAT4*)obj_ori, XMQuaternionIdentity());
	window.init(instance);
	d3d.init(window);
	aa_visualize_pt[0] = 506;
	aa_visualize_pt[1] = 111;
	use_fresnel = true;
	
	cout<<"window handle = " << window.handle<<endl;

	load_shaders();
	load_models();
	init_gbuffers(window.size().cx, window.size().cy);
	
	
	cout<<"window handle = " << window.handle<<endl;
    TwBar *bar = TwNewBar("Scene");

	obj_ori[0] = 0; obj_ori[1] = .93; obj_ori[2] = .37; obj_ori[3] = -0.07;
	use_custom_aa = true;
	TwAddVarRW(bar, "Orientation", TW_TYPE_QUAT4F, obj_ori, "opened=true axisy=y axisz=-z");
	TwAddVarRW(bar, "Dist", TW_TYPE_FLOAT, &cam_dist, "");
	//TwAddVarRW(bar, "Wireframe", TW_TYPE_BOOLCPP, &wireframe_mode, "");
	TwAddVarRW(bar, "Invert Depth", TW_TYPE_BOOLCPP, &invert_depth, "");
	TwAddVarRW(bar, "Frame", TW_TYPE_UINT32, &anim_frame, "");
	TwAddVarRW(bar, "GBuffer Debug", TW_TYPE_UINT32, &gbuffer_debug_mode, "min=0 max=3");
	TwAddVarRW(bar, "Blur Sigma", TW_TYPE_FLOAT, &blur_sigma, "min=1 max=9 step=0.05");

	TwAddVarRW(bar, "DX", TW_TYPE_FLOAT, &dx, "step=0.0005");
	
	TwAddVarRW(bar, "AA Viz X", TW_TYPE_FLOAT, &aa_visualize_pt[0], "");
	TwAddVarRW(bar, "AA Viz Y", TW_TYPE_FLOAT, &aa_visualize_pt[1], "");
	TwAddVarRW(bar, "AA Viz", TW_TYPE_BOOLCPP, &aa_visualize, "");
	light_dir_ws[0] = 0;
	light_dir_ws[1] = 1;
	light_dir_ws[2] = 0;
	TwAddVarRW(bar, "Use Fresnel", TW_TYPE_BOOLCPP, &use_fresnel, "");
	TwAddVarRW(bar, "Light Dir", TW_TYPE_DIR3F, light_dir_ws, "opened=true axisy=-y axisx=-x");
	cam_dist = 100;
	
	CD3D11_BLEND_DESC  additive_blend_state_desc(D3D11_DEFAULT);
	additive_blend_state_desc.RenderTarget[0].BlendEnable = true;
	additive_blend_state_desc.IndependentBlendEnable = false;
	additive_blend_state_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	additive_blend_state_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	additive_blend_state_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

	D3D11_DEPTH_STENCIL_DESC ds_desc;
	ZeroMemory(&ds_desc, sizeof(ds_desc));
	ds_desc.DepthFunc = D3D11_COMPARISON_GREATER;
	ds_desc.DepthEnable = true;
	ds_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	d3d.immediate_ctx->OMGetDepthStencilState(&ds_state, nullptr);
	d3d.device->CreateDepthStencilState(&ds_desc, &inverted_ds_state);

	
	cout<<"window handle = " << window.handle<<endl;
    
}
#include <iostream>
float dt = 0;
double gaussian(double sigma, double x)
{
	return exp(-(x*x) / (2 * sigma * sigma));
}
void build_gaussian_params(float sigma, 
						   int texture_size,
						   float* offsets,
						   float* norms,
						   int* offset_count)
{
	assert(offsets != nullptr);
	assert(texture_size > 0);
	assert(norms != nullptr);
	assert(offset_count != nullptr);
	assert(sigma >= 1);
	const float CUTOFF_MULTIPLIER = 3.0;
	const int MAX_TEXTURE_SAMPLES = 16;
	double sample_uv_size = (1.0f / (double)texture_size);
	//cutoff INCLUSIVE!
	//if cutoff says 9 it's saying there's 10 weights
	int cutoff = ceil(sigma * CUTOFF_MULTIPLIER);
	int texture_samples = 1 + ceil(cutoff / 2.0);
	int weights_count = (texture_samples - 1) * 2 + 1;
	assert(texture_samples < MAX_TEXTURE_SAMPLES);
	*offset_count = texture_samples;
	//DEAL WITH VP SIZE!
	double* weights = new double[weights_count];
	
	
	double total_weight = 0;
	for(int i = 0; i < weights_count; i++)
	{
		double weight = gaussian(sigma, i);
		weights[i] = weight;
		total_weight += (i > 0) ? 2 * weight : weight; //don't double count weight[0]
	}
	//do sample 0
	//offsets[0] = 0; //the original pixel's offset is implicit...
	norms[0] = weights[0] / total_weight;
	offsets[0] = 0; //writing this b/c of packing rules
	//start at sample 1..
	assert(texture_samples > 1);
	int base_offset = 1;
	for(int i = 1; i < texture_samples; i++)
	{
		double w_a = weights[base_offset + 2 * (i - 1)];
		double w_b = weights[base_offset + 2 * (i - 1) + 1];
		double pix_offset = 1 + 2*(i-1) + w_b/(w_a+w_b);
		offsets[i] = (float)(sample_uv_size * pix_offset);
		norms[i] = (float)((w_a + w_b) / total_weight);
	}
#ifdef DEBUG
	float norm_sum = norms[0];
	for(int i = 1; i < texture_samples; i++) norm_sum += 2 * norms[i];
	assert(abs(norm_sum - 1) < 0.00001);
#endif

}
template<typename T>
void clear_views(inout T** views, int num_views)
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
	if((window.size().cx != t2d_desc.Width) || (window.size().cy != t2d_desc.Height))
	{
		//recreate everything
		resize(&normal_rtv, &normal_srv, window.size().cx, window.size().cy);		
		resize(&albedo_rtv, &albedo_srv, window.size().cx, window.size().cy);
		resize(&debug_rtv[0], &(debug_srv[0]), window.size().cx, window.size().cy, &debug[0]);
		resize(&debug_rtv[1], &(debug_srv[1]), window.size().cx, window.size().cy, &debug[1]);
		resize(&debug_rtv[2], &(debug_srv[2]), window.size().cx, window.size().cy, &debug[2]);
	}

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

	fsquad_cb_data.debug_vars[0] = use_custom_aa;
	fsquad_cb_data.debug_vars[1] = (float)aa_visualize;
	fsquad_cb_data.debug_vars[2] = aa_visualize_pt[0];
	fsquad_cb_data.debug_vars[3] = aa_visualize_pt[1];

	gbuffer_debug_cb_data.render_mode = gbuffer_debug_mode;
	gbuffer_debug_cb_data.use_fresnel = use_fresnel ? 1 : 0;
	gbuffer_debug_cb_data.light_dir_ws[0] = 500 * light_dir_ws[0];
	gbuffer_debug_cb_data.light_dir_ws[1] = 500 * light_dir_ws[1];
	gbuffer_debug_cb_data.light_dir_ws[2] = 500 * light_dir_ws[2];
	gbuffer_debug_cb_data.light_dir_ws[3] = 1;
	
	auto joint_n = package.skeletal_animations[0].joints_count;
	auto frame_n = package.skeletal_animations[0].frames_count;
	int frame = anim_frame % frame_n;
	auto bone_data = &package.skeletal_animations[0].data[frame * joint_n];
	memcpy(&object_animation_cb_data, bone_data, sizeof(package::m44) * joint_n);
	
	
	SIZE window_size = window.size();

	auto w = XMMatrixRotationQuaternion(XMLoadFloat4((XMFLOAT4*)obj_ori));
	cam_focus[0] = dx;
	auto v = XMMatrixLookAtLH(XMVectorSet(cam_focus[0], cam_focus[1], cam_focus[2] - cam_dist, 1), 
		
	XMLoadFloat4((XMFLOAT4*)cam_focus), XMVectorSet(0, 1, 0, 0));


	
	//0 = f / (f - n)
	//1 = (-f * n) / (f - n)
	//near, far
	float n = 1; float f = 300; 

	if(invert_depth)
	{
		n = 300; f = 1; 
	}
	fsquad_cb_data.proj_constants[0] = f / (f - n);
	fsquad_cb_data.proj_constants[1] = (-f * n) / (f - n);
	auto p = XMMatrixPerspectiveFovLH(XM_PI / 4.0f, window_size.cx / (float)window_size.cy, n, f);//1, 1000);
	
	auto wv = XMMatrixMultiply(w, v);
	auto wvp = XMMatrixMultiply(wv, p);
	XMVECTOR v_det;
	XMStoreFloat4x4((XMFLOAT4X4*)&fsquad_cb_data.inv_p, XMMatrixTranspose(XMMatrixInverse(&v_det, p)));
	XMStoreFloat4x4((XMFLOAT4X4*)&fsquad_cb_data.proj, XMMatrixTranspose(p));
	XMStoreFloat4x4((XMFLOAT4X4*)&object_cb_data.wv, XMMatrixTranspose(wv));
	XMStoreFloat4x4((XMFLOAT4X4*)&object_cb_data.wvp, XMMatrixTranspose(wvp));
	XMStoreFloat4x4((XMFLOAT4X4*)&gbuffer_debug_cb_data.view, XMMatrixTranspose(v));

	d3d.sync_to_cbuffer(object_animation_cb, object_animation_cb_data);
	d3d.sync_to_cbuffer(gbuffer_debug_cb, gbuffer_debug_cb_data);
	d3d.sync_to_cbuffer(fsquad_cb, fsquad_cb_data);
	fx::update_gpu_env(&d3d, &fsquad_cb_data, &gpu_env);
	
	d3d.immediate_ctx->VSSetConstantBuffers(0, 1, &object_cb.p);
	d3d.immediate_ctx->VSSetConstantBuffers(1, 1, &object_animation_cb.p);
	d3d.immediate_ctx->PSSetConstantBuffers(0, 1, &object_cb.p);
	
	unsigned int offset = 0;
	d3d.immediate_ctx->IASetInputLayout(drawop.il);
	d3d.immediate_ctx->IASetVertexBuffers(0, 1, &drawop.vb, &drawop.vb_stride, &offset);
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
		nullptr, //ndc_tri_verts0_rtv,
		nullptr, //ndc_tri_verts1_rtv,
		nullptr, //ndc_tri_verts2_rtv
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
		d3d.immediate_ctx->PSSetShaderResources(0, srvs_count, srvs);
		d3d.immediate_ctx->DrawIndexed(mesh_part.indices_count, mesh_part.indices_offset, 0);		
	}
	
	unsigned int fsquad_stride = 3 * sizeof(float);
	//gbuffer
	if(1)
	{
		fx::shade_gbuffer(&d3d, &gpu_env, &shade_gbuffer_ctx, 
			albedo_srv, 
			normal_srv, 
			d3d.depth_srv, 
			&gbuffer_debug_cb_data, 
			debug_rtv[0]);		
	}
	if(1)
	{
		//highpass
		clear_views(rtvs, rtvs_count);
		clear_views(srvs, srvs_count);
		d3d.immediate_ctx->OMSetRenderTargets(rtvs_count, rtvs, nullptr);	
		d3d.immediate_ctx->PSSetShaderResources(0, srvs_count, srvs);


		rtvs[0] = debug_rtv[2];
		rtvs[1] = nullptr; 
		rtvs[2] = nullptr;

		srvs[0] = debug_srv[0];
		srvs[1] = nullptr;
		srvs[2] = nullptr;
				
		d3d::cbuffers::LumHighPassCb lum_highpass_cb_data;
		//lum_highpass_cb_data.min_lum[0] = 0;
		lum_highpass_cb_data.min_lum[0] = 2;
		d3d.sync_to_cbuffer(lum_highpass_cb, lum_highpass_cb_data);

		d3d.immediate_ctx->PSSetShaderResources(0, srvs_count, srvs);
		d3d.immediate_ctx->OMSetRenderTargets(rtvs_count, rtvs, nullptr);	
		
		d3d.immediate_ctx->PSSetConstantBuffers(0, 1, &lum_highpass_cb.p);
		d3d.immediate_ctx->IASetInputLayout(il_fsquad);
		d3d.immediate_ctx->IASetVertexBuffers(0, 1, &fsquad_vb.p, &fsquad_stride, &offset);
		d3d.immediate_ctx->VSSetShader(vs_lum_highpass, nullptr, 0);		
		d3d.immediate_ctx->PSSetShader(ps_lum_highpass, nullptr, 0);
		d3d.immediate_ctx->Draw(6, 0);
		if(1)
		{
			//blur
		
			clear_views(rtvs, rtvs_count);
			clear_views(srvs, srvs_count);
			d3d.immediate_ctx->OMSetRenderTargets(rtvs_count, rtvs, nullptr);	
			d3d.immediate_ctx->PSSetShaderResources(0, srvs_count, srvs);

			rtvs[0] = debug_rtv[1];
			rtvs[1] = nullptr; 
			rtvs[2] = nullptr;

			srvs[0] = debug_srv[2];
			srvs[1] = nullptr;
			srvs[2] = nullptr;
		
			d3d::cbuffers::BlurCB blur_cb_data;
			build_gaussian_params(blur_sigma,
				window.size().cx, 
				blur_cb_data.offsets, 
				blur_cb_data.norms, 
				&blur_cb_data.offsets_count[0]);
			d3d.sync_to_cbuffer(blur_cb, blur_cb_data);

			d3d.immediate_ctx->PSSetConstantBuffers(0, 1, &blur_cb.p);

		
			d3d.immediate_ctx->PSSetShaderResources(0, srvs_count, srvs);
			d3d.immediate_ctx->OMSetRenderTargets(rtvs_count, rtvs, nullptr);	

			d3d.immediate_ctx->IASetInputLayout(il_fsquad);
			d3d.immediate_ctx->IASetVertexBuffers(0, 1, &fsquad_vb.p, &fsquad_stride, &offset);
			d3d.immediate_ctx->VSSetShader(vs_blur, nullptr, 0);		
			d3d.immediate_ctx->PSSetShader(ps_blur_x, nullptr, 0);
			d3d.immediate_ctx->Draw(6, 0);
			//vertical
		
			clear_views(rtvs, rtvs_count);
			clear_views(srvs, srvs_count);
			d3d.immediate_ctx->OMSetRenderTargets(rtvs_count, rtvs, nullptr);	
			d3d.immediate_ctx->PSSetShaderResources(0, srvs_count, srvs);

			build_gaussian_params(blur_sigma,
				window.size().cy, 
				blur_cb_data.offsets, 
				blur_cb_data.norms, 
				&blur_cb_data.offsets_count[0]);
			d3d.sync_to_cbuffer(blur_cb, blur_cb_data);
			rtvs[0] = debug_rtv[2];
			rtvs[1] = nullptr; 
			rtvs[2] = nullptr;

			srvs[0] = debug_srv[1];
			srvs[1] = nullptr;
			srvs[2] = nullptr;
			d3d.immediate_ctx->PSSetShaderResources(0, srvs_count, srvs);
			d3d.immediate_ctx->OMSetRenderTargets(rtvs_count, rtvs, nullptr);	
		
			d3d.immediate_ctx->PSSetShader(ps_blur_y, nullptr, 0);

			d3d.immediate_ctx->Draw(6, 0);

			fx::resolve(&d3d, &gpu_env, &fx_env.resolve_ctx, debug_srv[0], debug_rtv[1]);

			fx::additive_blend(&d3d, &gpu_env, &fx_env.additive_blend_ctx, 
				debug_srv[2], debug_srv[1], d3d.back_buffer_rtv);
		}
	}
	ZeroMemory(rtvs, sizeof(rtvs));
	ZeroMemory(srvs, sizeof(srvs));


	d3d.swap_buffers();	
}

void GfxDemo::load_models()
{
	float fsquad_data[] = {
		-1, 1, 0,
		1, 1, 0,
		1, -1, 0,
		-1, 1, 0,
		1, -1, 0,
		-1, -1, 0
	};

	fsquad_vb = d3d.create_buffer((char*)fsquad_data, sizeof(fsquad_data), D3D11_BIND_VERTEX_BUFFER);
	if(0)
	{		
		model = asset::fbx::load_animated_fbx_model("assets/source/dude.fbx");
		//model = asset::fbx::load_animated_fbx_model("assets/source/cb.fbx");
	
	
		list<const Model*> models;
		models.push_back(model.get());
		package::bake_package(L"assets/ssr.package", &models);
	}
	
	cam_focus[0] = 0;//model->center[0];
	dx = 0;
	cam_focus[1] = 40;// model->center[1];
	cam_focus[2] = 0;//model->center[2];
	package::load_package(L"assets/ssr.package", &package);

	
	drawop = d3d.create_draw_op(
		package.meshes[0].verticies, 
		package.meshes[0].vertex_count, 
		package.meshes[0].vertex_stride, 
		(unsigned int*)package.meshes[0].indices, 
		package.meshes[0].indices_count, 
		il);

	for(int i = 0; i < package.textures.size(); i++)
	{
		ID3D11ShaderResourceView* srv;
		auto hr = D3DX11CreateShaderResourceViewFromMemory(
			d3d.device, 
			package.textures.at(i).data, 
			package.textures.at(0).byte_size, 
			nullptr,
			nullptr, 
			&srv,
			nullptr);
		textures.push_back(srv);
	}

	
	
}
void GfxDemo::load_shaders()
{
	//d3d.create_shaders_and_il(L"shaders/standard.hlsl", vs, ps, il, gfx::VertexTypes::eStandard);
	d3d.create_shaders_and_il(L"shaders/standard_animated.hlsl", &vs.p, &ps.p, nullptr, &il.p, 
		gfx::VertexTypes::eAnimatedStandard);
	d3d.create_shaders_and_il(L"shaders/shade_gbuffer.hlsl", &vs_shade_gbuffer.p, &ps_shade_gbuffer.p,
		 nullptr, 
		&il_fsquad.p, gfx::VertexTypes::eFSQuad);
	d3d.create_shaders_and_il(L"shaders/ssr.hlsl", &vs_ssao.p, &ps_ssao.p);

	auto vs_blob = d3d::load_shader(L"shaders/blur.hlsl", "vs", "vs_5_0");	
	d3d.device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &vs_blur.p);
	auto ps_blob = d3d::load_shader(L"shaders/blur.hlsl", "ps_blur_x", "ps_5_0");
	d3d.device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &ps_blur_x.p);
	ps_blob = d3d::load_shader(L"shaders/blur.hlsl", "ps_blur_y", "ps_5_0");
	d3d.device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &ps_blur_y.p);
	
	d3d.create_shaders_and_il(L"shaders/lum_highpass.hlsl", &vs_lum_highpass.p, &ps_lum_highpass.p);
	
	fsquad_cb = d3d.create_cbuffer<d3d::cbuffers::FSQuadCb>();
	blur_cb = d3d.create_cbuffer<d3d::cbuffers::BlurCB>();
	object_cb = d3d.create_cbuffer<d3d::cbuffers::ObjectCB>();
	object_animation_cb = d3d.create_cbuffer<d3d::cbuffers::ObjectAnimationCB>();
	gbuffer_debug_cb = d3d.create_cbuffer<d3d::cbuffers::ShadeGBufferDebugCB>();
	lum_highpass_cb = d3d.create_cbuffer<d3d::cbuffers::LumHighPassCb>();

	fx::make_shade_gbuffer_ctx(&d3d, &shade_gbuffer_ctx);
	fx::make_gpu_env(&d3d, &gpu_env);
	fx::make_fx_env(&d3d, &fx_env);
}