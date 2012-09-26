#pragma once
#include "GBuffer.h"
#include "d3d.h"
#include "gfx.h"
#include "fx.h"
#include "Camera.h"
#include "Effect.h"
class Renderer
{
public:
	Renderer();
	~Renderer(void);
	void initialize(D3D& pd3d);
	void beginFrame(int2 viewportSize);
	void drawStatic(const RuntimeMesh& mesh, const float4x4& worldTransform);
	void drawAnimated(const RuntimeMesh& mesh, 
		const float4x4& worldTransform, 
		const float4x4* jointTransforms, 
		int numJointTransforms);
	void deferredPass(float3 lightPosition);
	void postFxPass();
	void endFrame();
	Camera mainCamera;
private:
	void handleViewportResize(int2 viewportSize);
	void setupObjectData(const float4x4& worldTransform);
	void drawImpl(const RuntimeMesh& mesh);
	
private:	
	fx::Bokeh pointSpriteBokehDof;
	fx::DiffusionDof diffusionDof;
	fx::DiffusionDofCR diffusionDofCR;
	fx::VisualizeStructuredBuffer visualizeStructuredBuffer;
	Effect animatedGeometryDrawer;
	Effect standardGeometryDrawer;
	fx::GpuEnvironment gpuEnv;
	fx::FXEnvironment fxEnvironment;
	CBuffer<d3d::cbuffers::ObjectAnimationCB> objectAnimationCB;
	CBuffer<d3d::cbuffers::ObjectCB> objectCB;
	fx::FXContext shade_gbuffer_ctx;
	fx::FXContext tonemap_ctx;	
	D3D* d3d;
	GBuffer gbuffer;
	int2 cachedWindowSize;
	CComPtr<IFW1FontWrapper> drawtext;
};

