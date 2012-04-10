#include "stdafx.h"
#include "fx.h"

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
	int cutoff = (int)ceil(sigma * CUTOFF_MULTIPLIER);
	int texture_samples = 1 + (int)ceil(cutoff / 2.0);
	assert(texture_samples < MAX_TEXTURE_SAMPLES);
	assert(texture_samples > 1);
	int weights_count = (texture_samples - 1) * 2 + 1;
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
	norms[0] = (float)(weights[0] / total_weight);
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
	delete [] weights;
#ifdef DEBUG
	float norm_sum = norms[0];
	for(int i = 1; i < texture_samples; i++) norm_sum += 2 * norms[i];
	assert(abs(norm_sum - 1) < 0.00001);
#endif

}
namespace fx
{
	const int TARGETS_COUNT = 6;
	const int RESOURCES_COUNT = 6;
	const int UNIFORMS_COUNT = 4;
	void clear_shaders(Gfx* gfx)
	{
		gfx->immediate_ctx->VSSetShader(nullptr, nullptr, 0);
		gfx->immediate_ctx->PSSetShader(nullptr, nullptr, 0);
	}

	void make_fx_env(Gfx* gfx, out FXEnvironment* env)
	{
		zero(env);
		make_resolve_ctx(gfx, &env->resolve_ctx);
		make_blur_ctx(gfx, &env->blur_ctx);
		make_lum_highpass_ctx(gfx, &env->lum_highpass_ctx);
		make_additive_blend_ctx(gfx, &env->additive_blend_ctx);
		make_luminance_ctx(gfx, &env->luminance_ctx);
	}
	void make_gpu_env(Gfx* gfx, out GpuEnvironment* env)
	{
		zero(env);

		float default_blend_factors[4]; 
		unsigned int default_blend_mask;

		CD3D11_BLEND_DESC  additive_blend_state_desc(D3D11_DEFAULT);
		additive_blend_state_desc.RenderTarget[0].BlendEnable = true;
		additive_blend_state_desc.IndependentBlendEnable = false;
		additive_blend_state_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		additive_blend_state_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		additive_blend_state_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		additive_blend_state_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		additive_blend_state_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

		gfx->immediate_ctx->OMGetBlendState(&env->standard_blend, 
			default_blend_factors, 
			&default_blend_mask);
		gfx->device->CreateBlendState(&additive_blend_state_desc, &env->additive_blend);

		
		CD3D11_SAMPLER_DESC  linear_sampler_desc(D3D11_DEFAULT);
		gfx->device->CreateSamplerState(&linear_sampler_desc, &env->linear_sampler);
		
		//fsquad
		{
			
			env->fsquad_uniforms = gfx->create_cbuffer<d3d::cbuffers::FSQuadCb>();
			VertexShader* vs; PixelShader* ps;
	
			gfx->create_shaders_and_il(L"shaders/shade_gbuffer.hlsl", 
				&vs,
				&ps,
				nullptr, 
				&env->fsquad_il,
				gfx::VertexTypes::eFSQuad);
			env->fsquad_stride = 3 * sizeof(float);
			env->zero = 0;
			vs->Release(); 
			ps->Release();
			//fs quad vb
			{
				
				float fsquad_data[] = {
					-1, 1, 0,
					1, 1, 0,
					1, -1, 0,
					-1, 1, 0,
					1, -1, 0,
					-1, -1, 0
				};
				
					gfx->create_buffer((char*)fsquad_data, 
						sizeof(fsquad_data), 
					D3D11_BIND_VERTEX_BUFFER,
					&env->fsquad_vb.p);
				int ref = getref(env->fsquad_vb);
				d3d::name(env->fsquad_vb.p, "fsquad");
			}
		}
	}

	void make_gen_gbuffer_ctx(Gfx* gfx, out GenGBufferContext* ctx)
	{
		assert(false); //create a destroy() pair
		gfx->create_shaders_and_il(L"shaders/standard_animated.hlsl", 
			&ctx->vs, 
			&ctx->ps, 
			nullptr, 
			&ctx->il, 
			gfx::VertexTypes::eAnimatedStandard);
		ctx->object_uniforms = gfx->create_cbuffer<d3d::cbuffers::ObjectCB>();
		ctx->object_uniforms = gfx->create_cbuffer<d3d::cbuffers::ObjectAnimationCB>();
	}
	void gen_gbuffer(Gfx* gfx, 
		const GenGBufferContext* fx_ctx, 
		out Target* albedo, 
		out Target* normal,
		out Depth* depth)
	{
		//manually gen the gbuffer for now
		assert(false);
	}

	void make_shade_gbuffer_ctx(Gfx* gfx, out FXContext* ctx)
	{
		gfx->create_shaders_and_il(L"shaders/shade_gbuffer.hlsl", 
			&ctx->vs, 
			&ctx->ps,
			nullptr, 
			nullptr,
			gfx::VertexTypes::eFSQuad);	
		
		ctx->uniforms = gfx->create_cbuffer<d3d::cbuffers::ShadeGBufferDebugCB>();
	}
	void shade_gbuffer(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		in Resource* albedo, 
		in Resource* normal,
		in Resource* depth,
		in d3d::cbuffers::ShadeGBufferDebugCB* gbuffer_debug_cb,
		out Target* target)
	{
		gfx->sync_to_cbuffer(fx_ctx->uniforms, *gbuffer_debug_cb);

		Target* targets[TARGETS_COUNT] = {target};
		Resource* resources[RESOURCES_COUNT] = {normal, albedo, depth};
		Uniforms* uniforms[UNIFORMS_COUNT] = {fx_ctx->uniforms, gpu_env->fsquad_uniforms};

		gfx->immediate_ctx->OMSetRenderTargets(TARGETS_COUNT, targets, nullptr);
		gfx->immediate_ctx->PSSetShaderResources(0, RESOURCES_COUNT, resources);

		gfx->immediate_ctx->VSSetConstantBuffers(0, UNIFORMS_COUNT, uniforms);
		gfx->immediate_ctx->PSSetConstantBuffers(0, UNIFORMS_COUNT, uniforms);

		gfx->immediate_ctx->IASetInputLayout(gpu_env->fsquad_il);
		gfx->immediate_ctx->IASetVertexBuffers(0, 1, &gpu_env->fsquad_vb.p, &gpu_env->fsquad_stride, 
			&gpu_env->zero);

		gfx->immediate_ctx->VSSetShader(fx_ctx->vs, nullptr, 0);
		gfx->immediate_ctx->PSSetShader(fx_ctx->ps, nullptr, 0);
	
		gfx->immediate_ctx->Draw(6, 0);

	}

	void make_bloom_ctx(Gfx* gfx, out FXContext* ctx) { }
	void bloom(Gfx* gfx, 
		const GpuEnvironment* env,
		const FXContext* fx_ctx,
		in Resource* input,
		out Target* output);

	void make_blur_ctx(Gfx* gfx, out BlurContext* ctx)
	{		
		auto vs_blob = d3d::load_shader(L"shaders/blur.hlsl", "vs", "vs_5_0");	
		gfx->device->CreateVertexShader(
			vs_blob->GetBufferPointer(), 
			vs_blob->GetBufferSize(), 
			nullptr, 
			&ctx->vs);
		vs_blob->Release();
		auto ps_blob = d3d::load_shader(L"shaders/blur.hlsl", "ps_blur_x", "ps_5_0");
		gfx->device->CreatePixelShader(ps_blob->GetBufferPointer(), 
			ps_blob->GetBufferSize(), nullptr, &ctx->ps_h);
		ps_blob->Release();
		ps_blob = d3d::load_shader(L"shaders/blur.hlsl", "ps_blur_y", "ps_5_0");
		gfx->device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), 
			nullptr, &ctx->ps_v);
		ps_blob->Release();	

		ctx->uniforms = gfx->create_cbuffer<d3d::cbuffers::BlurCB>();
	}
	void blur(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const BlurContext* fx_ctx,
		BlurDirection direction,
		float sigma,
		in Resource* input,
		out Target* output)
	{
		Target* targets[TARGETS_COUNT] = {};
		Resource* resources[RESOURCES_COUNT] = {};
		gfx->immediate_ctx->OMSetRenderTargets(TARGETS_COUNT, targets, nullptr);	
		gfx->immediate_ctx->PSSetShaderResources(0, RESOURCES_COUNT, resources);
		targets[0] = output;
		resources[0] = input;
		gfx->immediate_ctx->OMSetRenderTargets(TARGETS_COUNT, targets, nullptr);	
		gfx->immediate_ctx->PSSetShaderResources(0, RESOURCES_COUNT, resources);
				
		d3d::cbuffers::BlurCB blur_cb_data;

		build_gaussian_params(sigma,
			direction == eHorizontal ? gpu_env->vp_w : gpu_env->vp_h, 
			blur_cb_data.offsets, 
			blur_cb_data.norms, 
			&blur_cb_data.offsets_count[0]);

		gfx->sync_to_cbuffer(fx_ctx->uniforms, blur_cb_data);
		gfx->immediate_ctx->PSSetSamplers(0, 1, &gpu_env->linear_sampler.p);
		gfx->immediate_ctx->PSSetConstantBuffers(0, 1, &fx_ctx->uniforms.p);

		

		
		gfx->immediate_ctx->IASetInputLayout(gpu_env->fsquad_il);
		gfx->immediate_ctx->IASetVertexBuffers(0, 1, &gpu_env->fsquad_vb.p, &gpu_env->fsquad_stride, 
			&gpu_env->zero);

		gfx->immediate_ctx->VSSetShader(fx_ctx->vs, nullptr, 0);
		gfx->immediate_ctx->PSSetShader(direction == eHorizontal ? fx_ctx->ps_h : fx_ctx->ps_v, nullptr, 0);
	
		gfx->immediate_ctx->Draw(6, 0);
	}

	void make_lum_highpass_ctx(Gfx* gfx, out FXContext* ctx)
	{		
		gfx->create_shaders_and_il(L"shaders/lum_highpass.hlsl", 
			&ctx->vs, &ctx->ps);
		ctx->uniforms = gfx->create_cbuffer<d3d::cbuffers::LumHighPassCb>();
	}
	void lum_highpass(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		float min_lum,
		in Resource* input,
		out Target* output)
	{
		Target* targets[TARGETS_COUNT] = {output};
		Resource* resources[RESOURCES_COUNT] = {input};
		
		d3d::cbuffers::LumHighPassCb lum_highpass_cb_data;
		lum_highpass_cb_data.min_lum[0] = min_lum;
		gfx->sync_to_cbuffer(fx_ctx->uniforms, lum_highpass_cb_data);

		gfx->immediate_ctx->OMSetRenderTargets(TARGETS_COUNT, targets, nullptr);
		gfx->immediate_ctx->PSSetShaderResources(0, RESOURCES_COUNT, resources);
		gfx->immediate_ctx->PSSetConstantBuffers(0, 1, &fx_ctx->uniforms.p);
		gfx->immediate_ctx->IASetInputLayout(gpu_env->fsquad_il);
		gfx->immediate_ctx->IASetVertexBuffers(0, 1, &gpu_env->fsquad_vb.p, &gpu_env->fsquad_stride, 
			&gpu_env->zero);

		gfx->immediate_ctx->VSSetShader(fx_ctx->vs, nullptr, 0);
		gfx->immediate_ctx->PSSetShader(fx_ctx->ps, nullptr, 0);
	
		gfx->immediate_ctx->Draw(6, 0);
	}
	
	void update_gpu_env(Gfx* gfx,		
		const d3d::cbuffers::FSQuadCb* fsquad_cb_data,
		GpuEnvironment* gpu_env)
	{
		//must call this
		gfx->sync_to_cbuffer(gpu_env->fsquad_uniforms, *fsquad_cb_data);
	}
	
	void make_additive_blend_ctx(Gfx* gfx, out FXContext* ctx)
	{		
		gfx->create_shaders_and_il(L"shaders/additive_blend.hlsl", 
			&ctx->vs, &ctx->ps);
	}
	void additive_blend(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		in Resource* a,
		in Resource* b,
		out Target* target)
	{
		Target* targets[TARGETS_COUNT] = {target};
		Resource* resources[RESOURCES_COUNT] = {a, b};

		gfx->immediate_ctx->OMSetRenderTargets(TARGETS_COUNT, targets, nullptr);
		gfx->immediate_ctx->PSSetShaderResources(0, RESOURCES_COUNT, resources);

		gfx->immediate_ctx->IASetInputLayout(gpu_env->fsquad_il);
		gfx->immediate_ctx->IASetVertexBuffers(0, 1, &gpu_env->fsquad_vb.p, &gpu_env->fsquad_stride, 
			&gpu_env->zero);

		gfx->immediate_ctx->VSSetShader(fx_ctx->vs, nullptr, 0);
		gfx->immediate_ctx->PSSetShader(fx_ctx->ps, nullptr, 0);
	
		gfx->immediate_ctx->Draw(6, 0);
	}
	
	void make_resolve_ctx(Gfx* gfx, out FXContext* ctx)
	{
		gfx->create_shaders_and_il(L"shaders/resolve.hlsl", 
			&ctx->vs, &ctx->ps);
	}
	void resolve(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		in Resource* source,
		out Target* target)
	{		
		Target* targets[TARGETS_COUNT] = {target};
		Resource* resources[RESOURCES_COUNT] = {source};
		
		gfx->immediate_ctx->PSSetShaderResources(0, RESOURCES_COUNT, resources);
		gfx->immediate_ctx->OMSetRenderTargets(TARGETS_COUNT, targets, nullptr);

		gfx->immediate_ctx->IASetInputLayout(gpu_env->fsquad_il);
		gfx->immediate_ctx->IASetVertexBuffers(0, 1, &gpu_env->fsquad_vb.p, &gpu_env->fsquad_stride, 
			&gpu_env->zero);

		gfx->immediate_ctx->VSSetShader(fx_ctx->vs, nullptr, 0);
		gfx->immediate_ctx->PSSetShader(fx_ctx->ps, nullptr, 0);
	
		gfx->immediate_ctx->Draw(6, 0);
	}

	void luminance( Gfx* gfx, 
		const GpuEnvironment* gpu_env, 
		const FXContext* fx_ctx, 
		in Resource* input, 
		out Target* output )
	{
		Target* targets[TARGETS_COUNT] = {output};
		Resource* resources[RESOURCES_COUNT] = {input};

		gfx->immediate_ctx->OMSetRenderTargets(TARGETS_COUNT, targets, nullptr);
		gfx->immediate_ctx->PSSetShaderResources(0, RESOURCES_COUNT, resources);

		gfx->immediate_ctx->IASetInputLayout(gpu_env->fsquad_il);
		gfx->immediate_ctx->IASetVertexBuffers(0, 1, &gpu_env->fsquad_vb.p, &gpu_env->fsquad_stride, 
			&gpu_env->zero);

		gfx->immediate_ctx->VSSetShader(fx_ctx->vs, nullptr, 0);
		gfx->immediate_ctx->PSSetShader(fx_ctx->ps, nullptr, 0);

		gfx->immediate_ctx->Draw(6, 0);
	}

	void make_luminance_ctx( Gfx* gfx, out FXContext* ctx )
	{
		gfx->create_shaders_and_il(L"shaders/luminance.hlsl", 
			&ctx->vs, &ctx->ps);
	}

	void make_tonemap_ctx( Gfx* gfx, out FXContext* ctx )
	{
		gfx->create_shaders_and_il(L"shaders/tonemap.hlsl", 
			&ctx->vs, &ctx->ps);
	}

	void tonemap( Gfx* gfx, const GpuEnvironment* gpu_env, const FXContext* fx_ctx, in Resource* input, out Target* output )
	{
		Target* targets[TARGETS_COUNT] = {output};
		Resource* resources[RESOURCES_COUNT] = {input};

		gfx->immediate_ctx->OMSetRenderTargets(TARGETS_COUNT, targets, nullptr);
		gfx->immediate_ctx->PSSetShaderResources(0, RESOURCES_COUNT, resources);

		gfx->immediate_ctx->IASetInputLayout(gpu_env->fsquad_il);
		gfx->immediate_ctx->IASetVertexBuffers(0, 1, &gpu_env->fsquad_vb.p, &gpu_env->fsquad_stride, 
			&gpu_env->zero);

		gfx->immediate_ctx->VSSetShader(fx_ctx->vs, nullptr, 0);
		gfx->immediate_ctx->PSSetShader(fx_ctx->ps, nullptr, 0);

		gfx->immediate_ctx->Draw(6, 0);
	}

};