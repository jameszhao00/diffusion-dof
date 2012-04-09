#pragma once
#include <string>
#include <locale>
#include "gfx.h"

struct Window;
#define MAX_BONES 64
#define MSAA_COUNT 4
#define MSAA_SRV_VIEW_DESC D3D11_SRV_DIMENSION_TEXTURE2DMS
#define MSAA_DSV_VIEW_DESC D3D11_DSV_DIMENSION_TEXTURE2DMS
namespace d3d
{
	ID3D10Blob* load_shader(const wchar_t * file, const char * entry, const char * profile);
	struct DrawOp
	{
		CComPtr<ID3D11Buffer> vb;
		CComPtr<ID3D11Buffer> ib;
		CComPtr<ID3D11InputLayout> il;
		unsigned int vb_stride;
		unsigned int ib_count;
	};
	namespace cbuffers
	{
		struct ShadeGBufferDebugCB
		{
			int render_mode;
			int use_fresnel;
			int padding[2];
			float view[4][4];
			float light_dir_ws[4];
		};
		struct FSQuadCb
		{
			float inv_p[4][4];
			//0 = f / (f - n)
			//1 = (-f * n) / (f - n)
			float proj_constants[4]; 
			float debug_vars[4];
			float proj[4][4];

		};
		struct ObjectCB
		{
			float wvp[4][4];
			float wv[4][4];
			unsigned int misc[4];
		};
		struct ObjectAnimationCB
		{
			float bone_transforms[MAX_BONES][4][4];
		};
		struct BlurCB
		{

			float offsets[16]; //for bilinear stuff - offsets[0] is the 1st pixel AFTER [i, j]
			//offsets are in UV coordinates
			float norms[16]; //normalization per weight
	
			int offsets_count[4]; //# of samples apart from img[i, j]
		};
		struct LumHighPassCb
		{
			float min_lum[4];
		};
	};
	template<typename T>
	void name(T* device_child, string name)
	{
		device_child->SetPrivateData( WKPDID_D3DDebugObjectName, name.length(), name.c_str());
	}
	template<typename T>
	void name(T* device_child, wstring ws)
	{
		//conversion code form http://stackoverflow.com/questions/4804298/c-how-to-convert-wstring-into-string
		std::string buffer(ws.size() * 4 + 1, 0);
		std::use_facet<std::ctype<wchar_t> >(std::locale("")).narrow(&ws[0], &ws[0] + ws.size(), '?', &buffer[0]);
		string name_param = std::string(&buffer[0]);
		name(device_child, name_param);
		//device_child->SetPrivateData( WKPDID_D3DDebugObjectName, small_name.length(), small_name.c_str());
	}
	template<typename T>
	string name(T* device_child)
	{
		unsigned int data_size = 64;
		char data[64];
		device_child->GetPrivateData( WKPDID_D3DDebugObjectName, &data_size, 
			(void*)data);
		return string(data, data_size);
	}
};
struct D3D
{
	void init(Window & window);
	void swap_buffers();
	
	template<typename T>
	ID3D11Buffer* create_buffer(const gfx::Data<T> * data, D3D11_BIND_FLAG bind_flag);
	ID3D11Buffer* create_buffer(const char* data, int byte_size, D3D11_BIND_FLAG bind_flag);
	template<typename T>
	ID3D11Buffer* create_cbuffer();	
	template<typename T>
	void sync_to_cbuffer(ID3D11Buffer * buffer, const T & data);

	void draw(const d3d::DrawOp & draw_op);

	void create_draw_op(
		const char* vb, 
		int vertex_count,
		int vb_stride,
		unsigned int* ib, 
		int indices_count,
		ID3D11InputLayout* il,
		d3d::DrawOp* drawop);

	//duplicate of the above... not using ccomptr
	void D3D::create_shaders_and_il(const wchar_t * file, 
		ID3D11VertexShader** vs, 
		ID3D11PixelShader** ps,
		ID3D11GeometryShader ** gs = nullptr,
		ID3D11InputLayout** il = nullptr,
		gfx::VertexTypes type = gfx::eUnknown);

	CComPtr<IDXGISwapChain> swap_chain;
	CComPtr<ID3D11Device> device;
	CComPtr<ID3D11DeviceContext> immediate_ctx;
	CComPtr<ID3D11RenderTargetView> back_buffer_rtv;
	CComPtr<ID3D11DepthStencilView> dsv;
	CComPtr<ID3D11ShaderResourceView> depth_srv;
	DXGI_SWAP_CHAIN_DESC swap_chain_desc;
	D3D11_TEXTURE2D_DESC depth_stencil_desc;
private:	
	void set_viewport(SIZE size);
	void window_resized(const Window * window);
};

template<typename T>
ID3D11Buffer* D3D::create_buffer(const gfx::Data<T> * data, D3D11_BIND_FLAG bind_flag)
{		
	return create_buffer((const char*)data->ptr, data->byte_size, bind_flag);
}
template<typename T>
ID3D11Buffer* D3D::create_cbuffer()
{		
	D3D11_BUFFER_DESC bd;
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(T);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bd.MiscFlags = 0;
	bd.StructureByteStride = 0;

	ID3D11Buffer* buffer;

	device->CreateBuffer(&bd, nullptr, &buffer);
	return buffer;
}
template<typename T>
void D3D::sync_to_cbuffer(ID3D11Buffer * buffer, const T & data)
{
	D3D11_MAPPED_SUBRESOURCE mapped_resource;
	immediate_ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);		
	memcpy(mapped_resource.pData, &data, sizeof(T));
	immediate_ctx->Unmap(buffer, 0);
}