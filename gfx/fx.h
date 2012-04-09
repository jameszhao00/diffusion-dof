#pragma once
#include <d3d11.h>
#include "d3d.h"
#include "gfx.h"
#define in
#define inout
#define out
template<typename T>
void zero(T* ptr)
{
	memset(ptr, 0, sizeof(T));
}
namespace fx
{
	struct FXContext
	{		
		VertexShader* vs;
		PixelShader* ps;
		Uniforms* uniforms;
	};
	struct GenGBufferContext 
	{ 
		ID3D11InputLayout* il;
		VertexShader* vs;
		PixelShader* ps;
		Uniforms* object_uniforms;
		Uniforms* animation_uniforms;
	};
	struct AdditiveBlendContext : FXContext { };
	struct ShadeGBufferContext : FXContext { };
	struct BloomContext { };
	struct SSRContext : FXContext { };
	struct BlurContext 
	{ 
		VertexShader* vs;
		PixelShader* ps_v; //vertical
		PixelShader* ps_h; //horizontal
		Uniforms* uniforms;
	};
	struct LumHighpassContext : FXContext { };

	enum BlurDirection
	{
		eVertical, eHorizontal
	};
	//global
	typedef D3D Gfx;
	typedef ID3D11RenderTargetView Depth;
	typedef ID3D11RenderTargetView Target;
	typedef ID3D11ShaderResourceView Resource;
	//blur, highpass, scale, ...
	struct FXEnvironment
	{
		BlurContext blur_ctx;
		FXContext resolve_ctx;
		LumHighpassContext lum_highpass_ctx;
		AdditiveBlendContext additive_blend_ctx;
	};
	//d3d states, etc
	struct GpuEnvironment
	{
		ID3D11SamplerState* linear_sampler;
		ID3D11BlendState* additive_blend;
		ID3D11BlendState* standard_blend;
		ID3D11InputLayout* fsquad_il;
		ID3D11Buffer* fsquad_vb;
		unsigned int fsquad_stride;
		unsigned int zero;
		Uniforms* fsquad_uniforms;
	};

	void make_fx_env(Gfx* gfx, out FXEnvironment* env);
	void make_gpu_env(Gfx* gfx, out GpuEnvironment* env);

	
	void make_resolve_ctx(Gfx* gfx, out FXContext* ctx);
	void make_additive_blend_ctx(Gfx* gfx, out AdditiveBlendContext* ctx);
	void make_gen_gbuffer_ctx(Gfx* gfx, out GenGBufferContext* ctx);
	void make_shade_gbuffer_ctx(Gfx* gfx, out ShadeGBufferContext* ctx);
	void make_bloom_ctx(Gfx* gfx, out BloomContext* ctx);
	void make_blur_ctx(Gfx* gfx, out BlurContext* ctx);
	void make_lum_highpass_ctx(Gfx* gfx, out LumHighpassContext* ctx);
	
	void resolve(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		in Resource* source,
		out Target* target);
	void additive_blend(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const AdditiveBlendContext* fx_ctx,
		in Resource* a,
		in Resource* b,
		out Target* target);
	void gen_gbuffer(Gfx* gfx, 
		const GenGBufferContext* fx_ctx, 
		out Target* albedo, 
		out Target* normal,
		out Depth* depth);
	void shade_gbuffer(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const ShadeGBufferContext* fx_ctx,
		in Resource* albedo, 
		in Resource* normal,
		in Resource* depth,
		in d3d::cbuffers::ShadeGBufferDebugCB* gbuffer_debug_cb,
		out Target* target);
	void bloom(Gfx* gfx, 
		const GpuEnvironment* env,
		const BloomContext* fx_ctx,
		in Resource* input,
		out Target* output);
	void blur(Gfx* gfx, 
		const GpuEnvironment* env,
		const BlurContext* fx_ctx,
		BlurDirection direction,
		float sigma,
		in Resource* input,
		out Target* output);
	void lum_highpass(Gfx* gfx, 
		const GpuEnvironment* env,
		const LumHighpassContext* fx_ctx,
		in Resource* input,
		out Resource* output);
	void update_gpu_env(Gfx* gfx,		
		const d3d::cbuffers::FSQuadCb* fsquad_cb_data,
		GpuEnvironment* gpu_env);
}