#include "stdafx.h"
#include "Renderer.h"


Renderer::Renderer()
{
}


Renderer::~Renderer(void)
{
}

void Renderer::initialize( D3D& pd3d )
{
	this->d3d = &pd3d;
	gbuffer.initialize(pd3d);

	objectCB.initialize(pd3d);
	objectAnimationCB.initialize(pd3d);

	gpuEnv.initialize(pd3d);
	fxEnvironment.initialize(pd3d);

	fx::make_shade_gbuffer_ctx(d3d, &shade_gbuffer_ctx);
	fx::make_tonemap_ctx(d3d, &tonemap_ctx);


	animatedGeometryDrawer.initialize(pd3d, L"shaders/standard_animated.hlsl", gfx::eAnimatedStandard);
	standardGeometryDrawer.initialize(pd3d, L"shaders/standard.hlsl", gfx::eStandard);
	pointSpriteBokehDof.initialize(d3d, &gpuEnv);
	diffusionDof.initialize(d3d, &gpuEnv, &fxEnvironment);
	diffusionDofCR.initialize(d3d, &gpuEnv, &fxEnvironment);
	visualizeStructuredBuffer.initialize(d3d, &gpuEnv, &fxEnvironment);

	IFW1Factory *drawtext_fac;
	HRESULT hResult = FW1CreateFactory(FW1_VERSION, &drawtext_fac);

	hResult = drawtext_fac->CreateFontWrapper(d3d->device, L"Arial", &drawtext);
	drawtext_fac->Release();
}

void Renderer::beginFrame( int2 viewportSize )
{
	gpuEnv.gfx_profiler.begin_frame();

	handleViewportResize(viewportSize);
	gpuEnv.vp_w = viewportSize.x;
	gpuEnv.vp_h = viewportSize.y;

	d3d::cbuffers::FSQuadCb fsquad_cb_data;
	fsquad_cb_data.proj_constants[0] = mainCamera.projectionConstants().x;
	fsquad_cb_data.proj_constants[1] = mainCamera.projectionConstants().y;
	fsquad_cb_data.inv_p = glm::inverse(mainCamera.projectionMatrix());
	fsquad_cb_data.proj = mainCamera.projectionMatrix();
	fx::update_gpu_env(d3d, &fsquad_cb_data, &gpuEnv);

	float white[] = {0, 0, 0, 0};
	d3d->immediate_ctx->ClearRenderTargetView(d3d->back_buffer_rtv, white);
	d3d->immediate_ctx->ClearDepthStencilView(d3d->dsv, D3D11_CLEAR_DEPTH, 0, 0);

	gbuffer.clearAll();

	d3d->immediate_ctx->OMSetDepthStencilState(gpuEnv.invertedDSS, 0);

	d3d->immediate_ctx->PSSetSamplers(0, 1, &gpuEnv.linear_sampler.p);
	d3d->immediate_ctx->PSSetSamplers(1, 1, &gpuEnv.aniso_sampler.p);
	gpuEnv.gfx_profiler.begin_block(L"geometry pass");
}

void Renderer::drawStatic( const RuntimeMesh& mesh, const float4x4& worldTransform )
{
	standardGeometryDrawer.apply(*d3d);
	d3d->immediate_ctx->IASetInputLayout(standardGeometryDrawer.il);
	setupObjectData(worldTransform);
	drawImpl(mesh);
}

void Renderer::drawAnimated( const RuntimeMesh& mesh, const float4x4& worldTransform, const float4x4* jointTransforms, int numJointTransforms )
{
	animatedGeometryDrawer.apply(*d3d);
	memcpy(objectAnimationCB.data.bone_transforms, jointTransforms, sizeof(float4x4) * numJointTransforms);			
	objectAnimationCB.sync();

	d3d->immediate_ctx->VSSetConstantBuffers(1, 1, &objectAnimationCB.cbuffer.p);
	setupObjectData(worldTransform);

	d3d->immediate_ctx->IASetInputLayout(animatedGeometryDrawer.il);

	drawImpl(mesh);
}

void Renderer::deferredPass( float3 lightPosition )
{
	gpuEnv.gfx_profiler.end_block();
	gpuEnv.gfx_profiler.begin_block(L"deferred pass");
	d3d::cbuffers::ShadeGBufferCB gbuffer_debug_cb_data;

	gbuffer_debug_cb_data.light_dir_ws[0] = lightPosition.x;
	gbuffer_debug_cb_data.light_dir_ws[1] = lightPosition.y;
	gbuffer_debug_cb_data.light_dir_ws[2] = lightPosition.z;
	gbuffer_debug_cb_data.light_dir_ws[3] = 1;
	gbuffer_debug_cb_data.view = mainCamera.viewMatrix();

	fx::shade_gbuffer(d3d, &gpuEnv, &shade_gbuffer_ctx, 
		gbuffer.albedo.srv,
		gbuffer.normal.srv,
		d3d->depth_srv,
		gbuffer.scratch[2].srv,
		&gbuffer_debug_cb_data,
		gbuffer.scratch[0].rtv);
		//d3d->back_buffer_rtv);
	gpuEnv.gfx_profiler.end_block();
}

void Renderer::postFxPass()
{
	gpuEnv.gfx_profiler.begin_block(L"ddof");

	//diffusionDof.execute(d3d, gbuffer.scratch[0].srv, d3d->depth_srv, 
	//	&gbuffer.scratchABC, &gbuffer.scratch[2]);

	diffusionDofCR.execute(d3d, gbuffer.scratch[0].srv, d3d->depth_srv, 
		&gbuffer.scratchABCD[0], &gbuffer.scratchABCD[1], &gbuffer.ddofOutputStructureBuffer);
	gpuEnv.gfx_profiler.end_block();
	visualizeStructuredBuffer.execute(d3d, &gbuffer.ddofOutputStructureBuffer, gbuffer.scratch[0].srv,
		d3d->back_buffer_rtv);
	gpuEnv.gfx_profiler.begin_block(L"tonemap");
	//pointSpriteBokehDof.execute(d3d, gbuffer.scratch[0].srv, d3d->depth_srv, gbuffer.scratch[1].rtv);
	//fx::tonemap(d3d, &gpuEnv, &tonemap_ctx, gbuffer.scratch[2].srv, d3d->back_buffer_rtv);
	gpuEnv.gfx_profiler.end_block();
}

void Renderer::endFrame()
{
	gpuEnv.gfx_profiler.drawStats(*d3d, drawtext);
	gpuEnv.gfx_profiler.end_frame();
	d3d->swap_buffers();
}

void Renderer::handleViewportResize( int2 viewportSize )
{
	assert(viewportSize.x > 0 && viewportSize.y > 0);
	if(viewportSize != cachedWindowSize)
	{
		mainCamera.ar = ((float)viewportSize.x)/viewportSize.y;
		cachedWindowSize = viewportSize;
		gbuffer.resize(viewportSize.x, viewportSize.y);
	}
}

void Renderer::setupObjectData( const float4x4& worldTransform )
{
	//setup object cb
	objectCB.data.wv = worldTransform * mainCamera.viewMatrix();
	objectCB.data.wvp = worldTransform * mainCamera.viewMatrix() * mainCamera.projectionMatrix();		
	objectCB.sync();	

	d3d->immediate_ctx->VSSetConstantBuffers(0, 1, &objectCB.cbuffer.p);
	d3d->immediate_ctx->PSSetConstantBuffers(0, 1, &objectCB.cbuffer.p);
}

void Renderer::drawImpl( const RuntimeMesh& mesh )
{
	unsigned int zero = 0;
	d3d->immediate_ctx->IASetVertexBuffers(0, 1, &mesh.live.vb.p, &mesh.live.vertexStride, &zero);
	d3d->immediate_ctx->IASetIndexBuffer(mesh.live.ib.p, DXGI_FORMAT_R32_UINT, 0);
	d3d->immediate_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11ShaderResourceView* srvs[] = {
		nullptr, 
		nullptr, 
		nullptr, 
		nullptr,
		nullptr,
		nullptr,
		nullptr
	};		
	d3d->immediate_ctx->PSSetShaderResources(0, sizeof(srvs)/sizeof(srvs[0]), srvs);
	ID3D11RenderTargetView* rtvs[] = {
		gbuffer.normal.rtv, 
		gbuffer.albedo.rtv,
		gbuffer.scratch[2].rtv,
		nullptr,
		nullptr,
		nullptr
	};

	d3d->immediate_ctx->OMSetRenderTargets(sizeof(rtvs)/sizeof(rtvs[0]), rtvs, d3d->dsv);		

	for(int i = 0; i < mesh.parts.size(); i++)
	{
		const auto& part = mesh.parts[i];
		if(part.textures.size() > 0)
		{				
			//textureSrvs should be a double ptr
			auto& textureSrvs = mesh.live.textures[part.textures[0]];
			assert(textureSrvs);
			d3d->immediate_ctx->PSSetShaderResources(0, 1, &textureSrvs.p);
		}

		d3d->immediate_ctx->PSSetSamplers(0, 1, &gpuEnv.linear_sampler.p);
		d3d->immediate_ctx->DrawIndexed(part.indexCount, part.indexOffset, 0);			
	}
}
