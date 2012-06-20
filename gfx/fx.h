#pragma once
#include <d3d11.h>
#include "d3d.h"
#include "gfx.h"
template<typename T>
void zero(T* ptr)
{
	memset(ptr, 0, sizeof(T));
}
namespace fx
{
	
	struct FXContext
	{		
		CComPtr<VertexShader> vs;
		CComPtr<PixelShader> ps;
		CComPtr<Uniforms> uniforms;
	};
	struct GenGBufferContext 
	{ 
		CComPtr<ID3D11InputLayout> il;
		CComPtr<VertexShader> vs;
		CComPtr<PixelShader> ps;
		CComPtr<Uniforms> object_uniforms;
		CComPtr<Uniforms> animation_uniforms;
	};
	struct BlurContext 
	{ 
		CComPtr<VertexShader> vs;
		CComPtr<PixelShader> ps_v; //vertical
		CComPtr<PixelShader> ps_h; //horizontal
		CComPtr<Uniforms> uniforms;
	};
	struct SSRContext
	{
		CComPtr<VertexShader> vs;
		//CComPtr<PixelShader> ps_old;
		CComPtr<PixelShader> ps_gen_samples;
		//CComPtr<PixelShader> ps_combine_samples;
		CComPtr<PixelShader> ps_shade;
		CComPtr<Uniforms> uniforms;
	};
	enum BlurDirection
	{
		eVertical, eHorizontal
	};
	//global
	typedef D3D Gfx;
	typedef ID3D11RenderTargetView Depth;
	typedef ID3D11RenderTargetView Target;
	typedef ID3D11ShaderResourceView Resource;
	typedef ID3D11UnorderedAccessView UAVResource;
	//blur, highpass, scale, ...


	struct FXEnvironment;
	struct GpuEnvironment;
	void make_fx_env(Gfx* gfx, FXEnvironment* env);
	void make_gpu_env(Gfx* gfx, GpuEnvironment* env);
	struct FXEnvironment
	{
		void initialize(D3D& d3d)
		{
			make_fx_env(&d3d, this);
		}
		BlurContext blur_ctx;
		FXContext resolve_ctx;
		FXContext lum_highpass_ctx;
		FXContext additive_blend_ctx;
		FXContext luminance_ctx;
	};
	//d3d states, etc
	struct GpuEnvironment
	{
		void initialize(D3D& d3d)
		{
			make_gpu_env(&d3d, this);
		}
		CComPtr<ID3D11DepthStencilState> invertedDSS;
		CComPtr<ID3D11DepthStencilState> notestDSS;
		CComPtr<ID3D11SamplerState> aniso_sampler;
		CComPtr<ID3D11SamplerState> linear_sampler;
		CComPtr<ID3D11BlendState> additive_blend;
		CComPtr<ID3D11BlendState> standard_blend;
		CComPtr<ID3D11InputLayout> fsquad_il;
		CComPtr<ID3D11Buffer> fsquad_vb;
		unsigned int fsquad_stride;
		unsigned int zero;
		CComPtr<Uniforms> fsquad_uniforms;

		GfxProfiler gfx_profiler;
		int vp_w, vp_h;
	};
	struct DiffusionDof
	{
		CBuffer<d3d::cbuffers::DDofCB> dofCB;
		FXEnvironment* fxEnvironment;
		GpuEnvironment* gpuEnvironment;
		CComPtr<ComputeShader> pass1H;
		CComPtr<ComputeShader> pass2H;
		CComPtr<ComputeShader> pass1V;
		CComPtr<ComputeShader> pass2V;
		bool gaussianBlur;

		void initialize(Gfx* gfx, GpuEnvironment* gpuEnvironment, FXEnvironment* fxEnvironment)
		{
			gaussianBlur = false;
			dofCB.initialize(*gfx);
			this->gpuEnvironment = gpuEnvironment;
			this->fxEnvironment = fxEnvironment;
			{
				CComPtr<ID3D10Blob> pass1Blob = d3d::load_shader(L"shaders/DiffusionDofPass1.hlsl", "csPass1H", "cs_5_0");			
				gfx->device->CreateComputeShader(pass1Blob->GetBufferPointer(), pass1Blob->GetBufferSize(), nullptr, &pass1H);
			}
			{
				CComPtr<ID3D10Blob> pass2Blob = d3d::load_shader(L"shaders/DiffusionDofPass2.hlsl", "csPass2H", "cs_5_0");			
				gfx->device->CreateComputeShader(pass2Blob->GetBufferPointer(), pass2Blob->GetBufferSize(), nullptr, &pass2H);
			}
			{
				CComPtr<ID3D10Blob> pass1Blob = d3d::load_shader(L"shaders/DiffusionDofPass1.hlsl", "csPass1V", "cs_5_0");			
				gfx->device->CreateComputeShader(pass1Blob->GetBufferPointer(), pass1Blob->GetBufferSize(), nullptr, &pass1V);
			}
			{
				CComPtr<ID3D10Blob> pass2Blob = d3d::load_shader(L"shaders/DiffusionDofPass2.hlsl", "csPass2V", "cs_5_0");			
				gfx->device->CreateComputeShader(pass2Blob->GetBufferPointer(), pass2Blob->GetBufferSize(), nullptr, &pass2V);
			}
			TwBar *bar = TwNewBar("Diffusion Dof");
			dofCB.data.coc.x = 30.f;
			TwAddVarRW(bar, "COC", TW_TYPE_FLOAT, 
				&dofCB.data.coc.x, "min=0.01 max=100 step=0.1");
			TwAddVarRW(bar, "Use Gaussian Blur", TW_TYPE_BOOLCPP, 
				&gaussianBlur, "");
		}
		void execute(Gfx* gfx, 
			Resource* inputColor, 
			Resource* inputDepth, 
			Texture2D* scratchABC,
			Texture2D* scratchD,
			Texture2D* outputDof1,
			Texture2D* outputDof2);
	};
	struct Bokeh
	{
		CComPtr<VertexShader> vs;
		CComPtr<PixelShader> ps;
		CComPtr<GeometryShader> gs;
		GpuEnvironment* gpuEnvironment;
		CBuffer<d3d::cbuffers::BokehCB> bokehCB;
		void initialize(Gfx* gfx, GpuEnvironment* gpuEnvironment) 
		{
			bokehCB.initialize(*gfx);
			this->gpuEnvironment = gpuEnvironment;
			gfx->create_shaders_and_il(L"shaders/Bokeh.hlsl", &vs.p, &ps.p, &gs.p);


			TwBar *bar = TwNewBar("Bokeh");
			bokehCB.data.cocPower.x = 30.f;
			TwAddVarRW(bar, "COC Power", TW_TYPE_FLOAT, 
				&bokehCB.data.cocPower, "");
		}
		void execute(Gfx* gfx, Resource* color, Resource* inputDepth, Target* outputDof );
	};

	void make_ssr_ctx(Gfx* gfx, SSRContext* ctx);
	void make_tonemap_ctx(Gfx* gfx, FXContext* ctx);
	void make_luminance_ctx(Gfx* gfx, FXContext* ctx);
	void make_resolve_ctx(Gfx* gfx, FXContext* ctx);
	void make_additive_blend_ctx(Gfx* gfx, FXContext* ctx);
	void make_gen_gbuffer_ctx(Gfx* gfx, GenGBufferContext* ctx);
	void make_shade_gbuffer_ctx(Gfx* gfx, FXContext* ctx);
	void make_bloom_ctx(Gfx* gfx, FXContext* ctx);
	void make_blur_ctx(Gfx* gfx, BlurContext* ctx);
	void make_lum_highpass_ctx(Gfx* gfx, FXContext* ctx);
	
	void resolve(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		Resource* source,
		Target* target);
	void additive_blend(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		Resource* a,
		Resource* b,
		Target* target);
	void gen_gbuffer(Gfx* gfx, 
		const GenGBufferContext* fx_ctx, 
		Target* albedo, 
		Target* normal,
		Depth* depth);
	void shade_gbuffer(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		Resource* albedo, 
		Resource* normal,
		Resource* depth,
		Resource* debugRef,
		d3d::cbuffers::ShadeGBufferCB* gbuffer_debug_cb,
		Target* target);
	void bloom(Gfx* gfx, 
		const GpuEnvironment* env,
		const FXContext* fx_ctx,
		Resource* input,
		Target* output);
	void blur(Gfx* gfx, 
		const GpuEnvironment* env,
		const BlurContext* fx_ctx,
		BlurDirection direction,
		float sigma,
		Resource* input,
		Target* output);
	void lum_highpass(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		float min_lum,
		Resource* input,
		Target* output);
	void luminance(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		Resource* input,
		Target* output);
	void tonemap(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		Resource* input,
		Target* output);
	void ssr(Gfx* gfx, 
		GpuEnvironment* gpu_env,
		const SSRContext* fx_ctx,
		Resource* normal,
		Resource* color,
		Resource* depth,
		Resource* noise,
		Resource* scratch0_r,
		Resource* scratch1_r,
		Target* scratch0_t,
		Target* scratch1_t,
		Target* output);
	void update_gpu_env(Gfx* gfx,		
		const d3d::cbuffers::FSQuadCb* fsquad_cb_data,
		GpuEnvironment* gpu_env);
}