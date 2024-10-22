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

	void make_fx_env(Gfx* gfx, FXEnvironment* env)
	{
		zero(env);
		make_resolve_ctx(gfx, &env->resolve_ctx);
		make_blur_ctx(gfx, &env->blur_ctx);
		make_lum_highpass_ctx(gfx, &env->lum_highpass_ctx);
		make_additive_blend_ctx(gfx, &env->additive_blend_ctx);
		make_luminance_ctx(gfx, &env->luminance_ctx);
	}
	void make_gpu_env(Gfx* gfx, GpuEnvironment* env)
	{
		env->gfx_profiler.init(gfx);

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
		linear_sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		linear_sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		gfx->device->CreateSamplerState(&linear_sampler_desc, &env->linear_sampler);
		linear_sampler_desc.MaxAnisotropy = 16;
		linear_sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
		gfx->device->CreateSamplerState(&linear_sampler_desc, &env->aniso_sampler);
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
		{
			//reversed dss
			D3D11_DEPTH_STENCIL_DESC ds_desc;
			ZeroMemory(&ds_desc, sizeof(ds_desc));
			ds_desc.DepthFunc = D3D11_COMPARISON_GREATER;
			ds_desc.DepthEnable = true;
			ds_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			gfx->device->CreateDepthStencilState(&ds_desc, &env->invertedDSS);	
		}
		{
			//no test dss
			D3D11_DEPTH_STENCIL_DESC ds_desc;
			ZeroMemory(&ds_desc, sizeof(ds_desc));
			ds_desc.DepthEnable = false;
			ds_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			gfx->device->CreateDepthStencilState(&ds_desc, &env->notestDSS);	
		}
	}

	void make_gen_gbuffer_ctx(Gfx* gfx, GenGBufferContext* ctx)
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
		Target* albedo, 
		Target* normal,
		Depth* depth)
	{
		//manually gen the gbuffer for now
		assert(false);
	}

	void make_shade_gbuffer_ctx(Gfx* gfx, FXContext* ctx)
	{
		gfx->create_shaders_and_il(L"shaders/shade_gbuffer.hlsl", 
			&ctx->vs, 
			&ctx->ps,
			nullptr, 
			nullptr,
			gfx::VertexTypes::eFSQuad);	
		
		ctx->uniforms = gfx->create_cbuffer<d3d::cbuffers::ShadeGBufferCB>();
	}
	void shade_gbuffer(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		Resource* albedo, 
		Resource* normal,
		Resource* depth,
		Resource* debugRef,
		d3d::cbuffers::ShadeGBufferCB* gbuffer_debug_cb,
		Target* target)
	{
		gfx->sync_to_cbuffer(fx_ctx->uniforms, *gbuffer_debug_cb);

		Target* targets[TARGETS_COUNT] = {target};
		Resource* resources[RESOURCES_COUNT] = {normal, albedo, depth, debugRef};
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

		//cleanup
		targets[0] = nullptr;
		gfx->immediate_ctx->OMSetRenderTargets(TARGETS_COUNT, targets, nullptr);

	}

	void make_bloom_ctx(Gfx* gfx, FXContext* ctx) { }
	void bloom(Gfx* gfx, 
		const GpuEnvironment* env,
		const FXContext* fx_ctx,
		Resource* input,
		Target* output);

	void make_blur_ctx(Gfx* gfx, BlurContext* ctx)
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
		Resource* input,
		Target* output)
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

	void make_lum_highpass_ctx(Gfx* gfx, FXContext* ctx)
	{		
		gfx->create_shaders_and_il(L"shaders/lum_highpass.hlsl", 
			&ctx->vs, &ctx->ps);
		ctx->uniforms = gfx->create_cbuffer<d3d::cbuffers::LumHighPassCb>();
	}
	void lum_highpass(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		float min_lum,
		Resource* input,
		Target* output)
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
	
	void make_additive_blend_ctx(Gfx* gfx, FXContext* ctx)
	{		
		gfx->create_shaders_and_il(L"shaders/additive_blend.hlsl", 
			&ctx->vs, &ctx->ps);
	}
	void additive_blend(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		Resource* a,
		Resource* b,
		Target* target)
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
	
	void make_resolve_ctx(Gfx* gfx, FXContext* ctx)
	{
		gfx->create_shaders_and_il(L"shaders/resolve.hlsl", 
			&ctx->vs, &ctx->ps);
	}
	void resolve(Gfx* gfx, 
		const GpuEnvironment* gpu_env,
		const FXContext* fx_ctx,
		Resource* source,
		Target* target)
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
		Resource* input, 
		Target* output )
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

	void make_luminance_ctx( Gfx* gfx, FXContext* ctx )
	{
		gfx->create_shaders_and_il(L"shaders/luminance.hlsl", 
			&ctx->vs, &ctx->ps);
	}

	void make_tonemap_ctx( Gfx* gfx, FXContext* ctx )
	{
		gfx->create_shaders_and_il(L"shaders/tonemap.hlsl", 
			&ctx->vs, &ctx->ps);
	}

	void tonemap( Gfx* gfx, const GpuEnvironment* gpu_env, const FXContext* fx_ctx, Resource* input, Target* output )
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

	void make_ssr_ctx( Gfx* gfx, SSRContext* ctx )
	{

		auto vs_blob = d3d::load_shader(L"shaders/ssr.hlsl", "vs", "vs_5_0");	
		gfx->device->CreateVertexShader(
			vs_blob->GetBufferPointer(), 
			vs_blob->GetBufferSize(), 
			nullptr, 
			&ctx->vs);
		vs_blob->Release();

		auto ps_blob = d3d::load_shader(L"shaders/ssr.hlsl", "gen_samples_ps", "ps_5_0");
		gfx->device->CreatePixelShader(ps_blob->GetBufferPointer(), 
			ps_blob->GetBufferSize(), nullptr, &ctx->ps_gen_samples);
		ps_blob->Release();
		ps_blob = d3d::load_shader(L"shaders/ssr.hlsl", "shade_ps", "ps_5_0");
		gfx->device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), 
			nullptr, &ctx->ps_shade);
		ps_blob->Release();	
		ctx->uniforms = gfx->create_cbuffer<d3d::cbuffers::FSQuadCb>();
	}

	void ssr( 
		Gfx* gfx, 
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
		Target* output )
	{
		gpu_env->gfx_profiler.begin_block(L"ssr pass 0");
		{
			//clear rtv/srv bindings
			Target* targets[TARGETS_COUNT] = {};
			Resource* resources[RESOURCES_COUNT] = {};
			gfx->immediate_ctx->OMSetRenderTargets(TARGETS_COUNT, targets, nullptr);
			gfx->immediate_ctx->PSSetShaderResources(0, RESOURCES_COUNT, resources);
		}

		{
			Target* targets[TARGETS_COUNT] = {scratch0_t};
			//Target* targets[TARGETS_COUNT] = {output};
			Resource* resources[RESOURCES_COUNT] = {normal, color, depth, noise};
			Uniforms* uniforms[UNIFORMS_COUNT] = {gpu_env->fsquad_uniforms};

			gfx->immediate_ctx->OMSetRenderTargets(TARGETS_COUNT, targets, nullptr);
			gfx->immediate_ctx->PSSetShaderResources(0, RESOURCES_COUNT, resources);

			gfx->immediate_ctx->VSSetConstantBuffers(0, UNIFORMS_COUNT, uniforms);
			gfx->immediate_ctx->PSSetConstantBuffers(0, UNIFORMS_COUNT, uniforms);

			gfx->immediate_ctx->IASetInputLayout(gpu_env->fsquad_il);
			gfx->immediate_ctx->IASetVertexBuffers(0, 1, &gpu_env->fsquad_vb.p, &gpu_env->fsquad_stride, 
				&gpu_env->zero);

			gfx->immediate_ctx->VSSetShader(fx_ctx->vs, nullptr, 0);
			gfx->immediate_ctx->PSSetShader(fx_ctx->ps_gen_samples, nullptr, 0);

			gfx->immediate_ctx->Draw(6, 0);
		}
		gpu_env->gfx_profiler.end_block();
		
		gpu_env->gfx_profiler.begin_block(L"ssr pass 1");
		if(1)
		{
			//clear rtv/srv bindings
			Target* targets[TARGETS_COUNT] = {};
			Resource* resources[RESOURCES_COUNT] = {};
			gfx->immediate_ctx->OMSetRenderTargets(TARGETS_COUNT, targets, nullptr);
			gfx->immediate_ctx->PSSetShaderResources(0, RESOURCES_COUNT, resources);
		}
		
		if(1)
		{
			Target* targets[TARGETS_COUNT] = {output};
			Resource* resources[RESOURCES_COUNT] = {normal, color, depth, noise, scratch0_r};

			gfx->immediate_ctx->OMSetRenderTargets(TARGETS_COUNT, targets, nullptr);
			gfx->immediate_ctx->PSSetShaderResources(0, RESOURCES_COUNT, resources);

			gfx->immediate_ctx->PSSetShader(fx_ctx->ps_shade, nullptr, 0);

			gfx->immediate_ctx->Draw(6, 0);
		}

		gpu_env->gfx_profiler.end_block();

		
	}



	void Bokeh::execute( Gfx* gfx, Resource* color, Resource* inputDepth, Target* outputDof )
	{
		Target* targets[TARGETS_COUNT] = {outputDof};
		Resource* resources[RESOURCES_COUNT] = {color, inputDepth};
		//set dss
		gfx->immediate_ctx->OMSetDepthStencilState(gpuEnvironment->notestDSS, 0);
		float blendFactor[] = {1, 1, 1, 1};
		//set blend
		gfx->immediate_ctx->OMSetBlendState(gpuEnvironment->additive_blend, blendFactor, 1);
		gfx->immediate_ctx->OMSetRenderTargets(TARGETS_COUNT, targets, nullptr);
		gfx->immediate_ctx->VSSetShaderResources(0, RESOURCES_COUNT, resources);
		gfx->immediate_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		gfx->immediate_ctx->IASetInputLayout(nullptr);
		bokehCB.sync();
		gfx->immediate_ctx->VSSetConstantBuffers(0, 1, &bokehCB.cbuffer.p);
		//gfx->immediate_ctx->IASetVertexBuffers(0, 1, nullptr, 0, 0);

		gfx->immediate_ctx->VSSetShader(vs, nullptr, 0);
		gfx->immediate_ctx->GSSetShader(gs, nullptr, 0);
		gfx->immediate_ctx->PSSetShader(ps, nullptr, 0);
		
		gfx->immediate_ctx->Draw(gpuEnvironment->vp_w * gpuEnvironment->vp_h, 0);

		gfx->immediate_ctx->GSSetShader(nullptr, nullptr, 0);
		targets[0] = nullptr;
		resources[0] = nullptr;
		resources[1] = nullptr;
		//restore dss
		gfx->immediate_ctx->OMSetDepthStencilState(gpuEnvironment->invertedDSS, 0);
		//restore blend
		gfx->immediate_ctx->OMSetBlendState(gpuEnvironment->standard_blend, blendFactor, 1);
		gfx->immediate_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		gfx->immediate_ctx->VSSetShaderResources(0, RESOURCES_COUNT, resources);
	}


	void DiffusionDof::execute( Gfx* gfx, 
		Resource* inputColor, 
		Resource* inputDepth, 
		Texture2D* scratchBCD,
		Texture2D* outputDof2)
	{
		{
			gfx->immediate_ctx->VSSetShader(nullptr, nullptr, 0);
			gfx->immediate_ctx->PSSetShader(nullptr, nullptr, 0);
			gfx->immediate_ctx->GSSetShader(nullptr, nullptr, 0);
			dofCB.sync();
			
			for(int i = 0; i < (int)dofCB.data.params.y; i++)
			{
				//horizontal
				{
					gfx->immediate_ctx->CSSetConstantBuffers(0, 1, &dofCB.cbuffer.p);
					gfx->immediate_ctx->CSSetConstantBuffers(1, 1, &gpuEnvironment->fsquad_uniforms.p);
					int numThreadGroups = (int)ceil(gpuEnvironment->vp_h / 32.f);
					//call pass 1
					{
						gfx->immediate_ctx->CSSetShader(pass1H, nullptr, 0);
						Resource* resources[] = {inputDepth, outputDof2->srv};
						if(i == 0)
						{
							resources[1] = inputColor;
						}
						gfx->immediate_ctx->CSSetShaderResources(0, 2, resources);
						UAVResource* uavs[] = {scratchBCD->uav};
						gfx->immediate_ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
						gfx->immediate_ctx->Dispatch(numThreadGroups, 1, 1);
					}
					//cleanup
					{
						Resource* resources[] = {nullptr, nullptr, nullptr};
						UAVResource* uavs[] = {nullptr, nullptr, nullptr};
						gfx->immediate_ctx->CSSetShaderResources(0, 3, resources);
						gfx->immediate_ctx->CSSetUnorderedAccessViews(0, 3, uavs, nullptr);
					}
					//call pass 2
					{
						gfx->immediate_ctx->CSSetShader(pass2H, nullptr, 0);
						Resource* resources[] = {scratchBCD->srv, inputColor};
						gfx->immediate_ctx->CSSetShaderResources(0, 2, resources);
						UAVResource* uavs[] = {outputDof2->uav};
						gfx->immediate_ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
						gfx->immediate_ctx->Dispatch(numThreadGroups, 1, 1);
					}
					//cleanup
					{
						Resource* resources[] = {nullptr, nullptr};
						UAVResource* uavs[] = {nullptr, nullptr};
						gfx->immediate_ctx->CSSetShaderResources(0, 2, resources);
						gfx->immediate_ctx->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
					}
				}
				//vertical
				{
					gfx->immediate_ctx->CSSetConstantBuffers(0, 1, &dofCB.cbuffer.p);
					int numThreadGroups = (int)ceil(gpuEnvironment->vp_w / 32.f);
					//call pass 1
					{
						gfx->immediate_ctx->CSSetShader(pass1V, nullptr, 0);
						Resource* resources[] = {inputDepth, outputDof2->srv};
						gfx->immediate_ctx->CSSetShaderResources(0, 2, resources);
						UAVResource* uavs[] = {scratchBCD->uav};
						gfx->immediate_ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
						gfx->immediate_ctx->Dispatch(numThreadGroups, 1, 1);
					}
					//cleanup
					{
						Resource* resources[] = {nullptr, nullptr};
						UAVResource* uavs[] = {nullptr, nullptr};
						gfx->immediate_ctx->CSSetShaderResources(0, 2, resources);
						gfx->immediate_ctx->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
					}
					//call pass 2
					{
						gfx->immediate_ctx->CSSetShader(pass2V, nullptr, 0);
						Resource* resources[] = {scratchBCD->srv};
						gfx->immediate_ctx->CSSetShaderResources(0, 1, resources);
						UAVResource* uavs[] = {outputDof2->uav};
						gfx->immediate_ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
						gfx->immediate_ctx->Dispatch(numThreadGroups, 1, 1);
					}
					//cleanup
					{
						Resource* resources[] = {nullptr, nullptr};
						UAVResource* uavs[] = {nullptr, nullptr};
						gfx->immediate_ctx->CSSetShaderResources(0, 2, resources);
						gfx->immediate_ctx->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
					}
				}
			}
		}
	}

#define PCR_TG_SIZE_X 64
	void DiffusionDofCR::execute( Gfx* gfx, 
		Resource* inputColor, 
		Resource* inputDepth, 
		Texture2D* scratchABCD1,
		Texture2D* scratchABCD2,
		Texture2D* outputDof)
	{
		{
			gfx->immediate_ctx->VSSetShader(nullptr, nullptr, 0);
			gfx->immediate_ctx->PSSetShader(nullptr, nullptr, 0);
			gfx->immediate_ctx->GSSetShader(nullptr, nullptr, 0);

			dofCB.data.params2.x = gpuEnvironment->vp_w;
			dofCB.data.params2.y = gpuEnvironment->vp_h;

			gfx->immediate_ctx->CSSetConstantBuffers(0, 1, &dofCB.cbuffer.p);
			gfx->immediate_ctx->CSSetConstantBuffers(1, 1, &gpuEnvironment->fsquad_uniforms.p);
			Texture2D* scratchABCDs[] = {scratchABCD1, scratchABCD2};
			{
				int passesCount = glm::ceil(log2((float)gpuEnvironment->vp_w / PCR_TG_SIZE_X));
				//pass 0 and 1 collapsed into pass 1
				for(int passIdx = 1; passIdx < passesCount; passIdx++)
				{

					dofCB.data.params.z = passIdx;
					dofCB.sync();
					//TODO: don't create hABCDs[0]
					UAVResource* uavs[1] = {hABCDs[passIdx].uav};
					gfx->immediate_ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
					if(passIdx == 1)
					{		
						Resource* resources[3] = {inputDepth, inputColor, nullptr};
						gfx->immediate_ctx->CSSetShaderResources(0, 3, resources);	
						gfx->immediate_ctx->CSSetShader(pass1HP0, nullptr, 0);
					}
					else
					{
						Resource* resources[3] = {nullptr, nullptr, hABCDs[passIdx-1].srv};
						gfx->immediate_ctx->CSSetShaderResources(0, 3, resources);	
						gfx->immediate_ctx->CSSetShader(pass1H, nullptr, 0);
					}

					float2 numThreads(entriesAtPass(gpuEnvironment->vp_w, passIdx), gpuEnvironment->vp_h);
					float2 numGroups(glm::ceil(numThreads / float2(passIdx == 1 ? 64-2 : 64-1, 1)));
					gfx->immediate_ctx->Dispatch((int)numGroups.x, (int)numGroups.y, 1);
					
					
				}
				{
					//run pcr
					UAVResource* uavs[1] = {hYs[passesCount - 1].uav};
					gfx->immediate_ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

					Resource* resources[3] = {hABCDs[passesCount - 1].srv, nullptr, nullptr};
					gfx->immediate_ctx->CSSetShaderResources(0, 3, resources);	

					gfx->immediate_ctx->CSSetShader(pcr, nullptr, 0);

					gfx->immediate_ctx->Dispatch(1, gpuEnvironment->vp_h, 1);
				}
				
				//cleanup
				if(1)
				{
					Resource* resources[] = {nullptr, nullptr, nullptr, nullptr};
					UAVResource* uavs[] = {nullptr, nullptr, nullptr};
					gfx->immediate_ctx->PSSetShaderResources(0, 4, resources);
					gfx->immediate_ctx->CSSetShaderResources(0, 4, resources);
					gfx->immediate_ctx->CSSetUnorderedAccessViews(0, 3, uavs, nullptr);
				}
				//pass -1 and 0 collapsed into pass 0
				//start at passCount + 2 as pcr essentially solves a pass 
				for(int passIdx = passesCount - 2; passIdx > -1; passIdx--)
				{
					dofCB.data.params.z = passIdx;
					dofCB.sync();

					if(passIdx == 0)
					{
						UAVResource* uavs[] = {outputDof->uav, nullptr, nullptr};
						gfx->immediate_ctx->CSSetUnorderedAccessViews(0, 3, uavs, nullptr);
					}
					else
					{
						UAVResource* uavs[] = {hYs[passIdx].uav, nullptr, nullptr};
						gfx->immediate_ctx->CSSetUnorderedAccessViews(0, 3, uavs, nullptr);
					}
					Resource* resources[4] = {nullptr, nullptr, nullptr, nullptr};	
					if(passIdx == 0)
					{
						resources[1] = inputDepth;
						resources[2] = inputColor;
						resources[3] = hYs[1].srv;
						gfx->immediate_ctx->CSSetShader(pass2HFirstPass, nullptr, 0);
					}
					else
					{
						resources[0] = hABCDs[passIdx].srv;
						resources[3] = hYs[passIdx + 1].srv;
						gfx->immediate_ctx->CSSetShader(pass2H, nullptr, 0);	
					}					
					gfx->immediate_ctx->CSSetShaderResources(0, 4, resources);
					
					//number of items at current pass - number of items @ current pass + 1

					float2 numThreads(
						entriesAtPass(gpuEnvironment->vp_w, passIdx)
						- entriesAtPass(gpuEnvironment->vp_w, passIdx + 1), gpuEnvironment->vp_h
						);
					float2 numGroups(glm::ceil(numThreads / (passIdx == 0 ? float2(16, 4) : float2(16))));
					gfx->immediate_ctx->Dispatch((int)numGroups.x, (int)numGroups.y, 1);
				}
				
				//cleanup
				{
					Resource* resources[] = {nullptr, nullptr, nullptr, nullptr};
					UAVResource* uavs[] = {nullptr, nullptr, nullptr};
					gfx->immediate_ctx->PSSetShaderResources(0, 4, resources);
					gfx->immediate_ctx->CSSetShaderResources(0, 4, resources);
					gfx->immediate_ctx->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
				}	
			}
		}
	}
};