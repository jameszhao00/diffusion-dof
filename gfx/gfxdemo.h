#pragma once

#include "window.h"
#include "d3d.h"
#include "gfx.h"
#include "package.h"
#include <vector>
#include "fx.h"
#include "fbx.h"

using namespace gfx;
using namespace std;
struct GfxDemo
{
	void init(HINSTANCE instance);
	void init_gbuffers(int w, int h);
	void init_aa_buffers(int w, int h);
	void destroy();
	void destroy_gbuffers();
	void load_shaders();
	void load_models();
	void frame();
	
	float cam_focus[4];

	CComPtr<ID3D11VertexShader> vs;
	CComPtr<ID3D11PixelShader> ps;
	CComPtr<ID3D11GeometryShader> gs;
	CComPtr<ID3D11InputLayout> il;
	
	CComPtr<ID3D11Buffer> object_cb;
	CComPtr<ID3D11Buffer> object_animation_cb;
	

	CComPtr<ID3D11ShaderResourceView> vb_srv;
	CComPtr<ID3D11ShaderResourceView> ib_srv;

	int gbuffer_debug_mode;
	
	//ID3D11Texture2D* normal;
	CComPtr<ID3D11ShaderResourceView> normal_srv;
	CComPtr<ID3D11RenderTargetView> normal_rtv;
	CComPtr<ID3D11Texture2D> normal;
	//ID3D11Texture2D* albedo;
	CComPtr<ID3D11ShaderResourceView> albedo_srv;
	CComPtr<ID3D11RenderTargetView> albedo_rtv;
	CComPtr<ID3D11Texture2D> albedo;
	//ID3D11Texture2D* debug;
	CComPtr<ID3D11ShaderResourceView> debug_srv[3];
	CComPtr<ID3D11RenderTargetView> debug_rtv[3];
	CComPtr<ID3D11Texture2D> debug[3];
	
	CComPtr<ID3D11DepthStencilState> ds_state; 
	CComPtr<ID3D11DepthStencilState> inverted_ds_state; 

	bool invert_depth;
	d3d::DrawOp drawop;
	package::Package package;
	shared_ptr<Model> model;

	vector<CComPtr<ID3D11ShaderResourceView>> textures;
	float blur_sigma;

	float light_dir_ws[3];
	bool aa_visualize;
	float dx;
	bool do_anim;
	unsigned int anim_frame;

	Window window;
	D3D d3d;
	bool use_fresnel;
	bool is_top;

	float obj_ori[4];
	float cam_dist;
	int vx; int vy;
	fx::GpuEnvironment gpu_env;
	fx::FXEnvironment fx_env;
	fx::FXContext shade_gbuffer_ctx;
	fx::FXContext tonemap_ctx;
	fx::SSRContext ssr_ctx;
	char* save_name;
	ID3D11ShaderResourceView* noise;

	vector<asset::fbx::Camera> cameras;
	int camera_i;
};