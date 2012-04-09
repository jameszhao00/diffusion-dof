#pragma once

#include "window.h"
#include "d3d.h"
#include "gfx.h"
#include "package.h"
#include <vector>
#include "fx.h"

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

	CComPtr<ID3D11VertexShader> vs_shade_gbuffer; 
	CComPtr<ID3D11PixelShader> ps_shade_gbuffer;
		
	CComPtr<ID3D11VertexShader> vs_ssao; 
	CComPtr<ID3D11PixelShader> ps_ssao;

	CComPtr<ID3D11VertexShader> vs_lum_highpass; 
	CComPtr<ID3D11PixelShader> ps_lum_highpass;
	
	CComPtr<ID3D11VertexShader> vs_blur; 
	CComPtr<ID3D11PixelShader> ps_blur_x;
	CComPtr<ID3D11PixelShader> ps_blur_y;

	CComPtr<ID3D11InputLayout> il_fsquad;
	
	CComPtr<ID3D11Buffer> lum_highpass_cb;
	CComPtr<ID3D11Buffer> blur_cb;
	CComPtr<ID3D11Buffer> fsquad_cb;
	CComPtr<ID3D11Buffer> object_cb;
	CComPtr<ID3D11Buffer> object_animation_cb;
	CComPtr<ID3D11Buffer> gbuffer_debug_cb;
	CComPtr<ID3D11RasterizerState> wireframe_rasterizer; 
	CComPtr<ID3D11SamplerState> sampler; 
	

	ID3D11ShaderResourceView* vb_srv;
	ID3D11ShaderResourceView* ib_srv;

	int gbuffer_debug_mode;
	
	//ID3D11Texture2D* normal;
	ID3D11ShaderResourceView* normal_srv;
	ID3D11RenderTargetView* normal_rtv;
	//ID3D11Texture2D* albedo;
	ID3D11ShaderResourceView* albedo_srv;
	ID3D11RenderTargetView* albedo_rtv;
	//ID3D11Texture2D* debug;
	ID3D11ShaderResourceView* debug_srv[3];
	ID3D11RenderTargetView* debug_rtv[3];
	ID3D11Texture2D* debug[3];
	
	ID3D11DepthStencilState* ds_state; 
	ID3D11DepthStencilState* inverted_ds_state; 

	bool invert_depth;
	CComPtr<ID3D11Buffer> fsquad_vb;
	d3d::DrawOp drawop;
	package::Package package;
	shared_ptr<Model> model;

	vector<CComPtr<ID3D11ShaderResourceView>> textures;
	float blur_sigma;
	float cam_x;
	bool use_custom_aa;

	float aa_visualize_pt[2];
	float light_dir_ws[3];
	bool aa_visualize;
	float dx;

	unsigned int anim_frame;

	Window window;
	D3D d3d;
	bool use_fresnel;

	float obj_ori[4];
	float cam_dist;
	bool wireframe_mode;

	unsigned int frame_i;

	fx::GpuEnvironment gpu_env;
	fx::FXEnvironment fx_env;
	fx::ShadeGBufferContext shade_gbuffer_ctx;
};