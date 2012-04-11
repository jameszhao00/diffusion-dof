#include "stdafx.h"

#include <D3DCompiler.h>

#include "d3d.h"
#include "window.h"
int getref(IUnknown* ptr)
{
	ptr->AddRef();
	return ptr->Release();
}
DXGI_SWAP_CHAIN_DESC make_swap_chain_desc(const Window & window)
{
	DXGI_SWAP_CHAIN_DESC swap_chain_desc;
	ZeroMemory(&swap_chain_desc, sizeof(swap_chain_desc));
	swap_chain_desc.BufferCount = 1;
	swap_chain_desc.BufferDesc.Width = window.size().cx;
	swap_chain_desc.BufferDesc.Height = window.size().cy;
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	swap_chain_desc.BufferDesc.RefreshRate.Numerator = 0;
	swap_chain_desc.BufferDesc.RefreshRate.Denominator = 0;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.OutputWindow = window.handle;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.Windowed = TRUE;
	swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	return swap_chain_desc;
}
D3D11_TEXTURE2D_DESC make_depth_stencil_desc(SIZE size)
{	
	D3D11_TEXTURE2D_DESC depth_stencil_desc;
	depth_stencil_desc.Width = size.cx;
	depth_stencil_desc.Height = size.cy;
	depth_stencil_desc.MipLevels = 1;
	depth_stencil_desc.ArraySize = 1;
	depth_stencil_desc.Format = DXGI_FORMAT_R32_TYPELESS;
	depth_stencil_desc.SampleDesc.Count = MSAA_COUNT;
	depth_stencil_desc.SampleDesc.Quality = 0;
	depth_stencil_desc.Usage = D3D11_USAGE_DEFAULT;
	depth_stencil_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	depth_stencil_desc.CPUAccessFlags = 0;
	depth_stencil_desc.MiscFlags = 0;

	return depth_stencil_desc;
}
D3D11_DEPTH_STENCIL_VIEW_DESC make_depth_dsv_desc()
{
	D3D11_DEPTH_STENCIL_VIEW_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.ViewDimension = MSAA_DSV_VIEW_DESC;
	desc.Format = DXGI_FORMAT_D32_FLOAT;
	return desc;
}
D3D11_SHADER_RESOURCE_VIEW_DESC make_depth_srv_desc()
{
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.ViewDimension = MSAA_SRV_VIEW_DESC;
	desc.Texture2D.MostDetailedMip = 0;
	desc.Texture2D.MipLevels = 1;
	desc.Format = DXGI_FORMAT_R32_FLOAT;
	return desc;
}

void D3D::create_draw_op(
		const char* vb, 
		int vertex_count,
		int vb_stride,
		unsigned int* ib, 
		int indices_count,
		ID3D11InputLayout* il,
		d3d::DrawOp* draw_op)
{
	//WARNING: D3D11_BIND_SHADER_RESOURCE not usually necessary...
	//just for testing
	create_buffer(vb, vb_stride * vertex_count,
		(D3D11_BIND_FLAG)( D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE), &draw_op->vb.p);

	d3d::name(draw_op->vb.p, "drawop vb");
	create_buffer((char*)ib, sizeof(unsigned int) * indices_count, 
		(D3D11_BIND_FLAG)( D3D11_BIND_INDEX_BUFFER | D3D11_BIND_SHADER_RESOURCE), &draw_op->ib.p);
	d3d::name(draw_op->ib.p, "drawop ib");
	draw_op->ib_count = indices_count;
	draw_op->vb_stride = vb_stride;
	draw_op->il.Attach(il);
}

void D3D::create_buffer(const char* data, int byte_size, D3D11_BIND_FLAG bind_flag, ID3D11Buffer** buf)
{
	D3D11_BUFFER_DESC bd;
	bd.Usage = D3D11_USAGE_IMMUTABLE;
	bd.ByteWidth = byte_size;
	bd.BindFlags = bind_flag;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	bd.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA initial_data;
	initial_data.pSysMem = data;
	initial_data.SysMemPitch = 0;
	initial_data.SysMemSlicePitch = 0;

	device->CreateBuffer(&bd, &initial_data, buf);
	int r = getref(*buf);
	
}

void D3D::init(Window & window)
{
	swap_chain_desc = make_swap_chain_desc(window);

	D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG | D3D11_RLDO_DETAIL, 
									NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc, &swap_chain.p, 
									&device.p, NULL, &immediate_ctx.p);
		
	window.resize_callback = std::bind(&D3D::window_resized, this, std::placeholders::_1);

	ID3D11Texture2D * back_buffer;
	swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&back_buffer);
	d3d::name(back_buffer, "backbuffer");
	device->CreateRenderTargetView(back_buffer, NULL, &back_buffer_rtv.p);
	d3d::name(back_buffer_rtv.p, "backbuffer rtv");
	back_buffer->Release();


	ID3D11Texture2D * depth_texture;
	depth_stencil_desc = make_depth_stencil_desc(window.size());

	device->CreateTexture2D(&depth_stencil_desc, NULL, &depth_texture);
	d3d::name(depth_texture, "depth");

	auto depth_dsv_desc = make_depth_dsv_desc();
	auto depth_srv_desc = make_depth_srv_desc();

	device->CreateDepthStencilView(depth_texture, &depth_dsv_desc, &dsv.p);
	
	d3d::name(dsv.p, "depth dsv");
	device->CreateShaderResourceView(depth_texture, &depth_srv_desc, &depth_srv.p);
	d3d::name(depth_srv.p, "depth srv");
		

	depth_texture->Release();

	set_viewport(window.size());

	TwInit(TW_DIRECT3D11, device);
	
}

void D3D::set_viewport(SIZE size)
{		
	D3D11_VIEWPORT vp;
	vp.Width = (float)size.cx;
	vp.Height = (float)size.cy;
	//max depth has to be greater than min
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	immediate_ctx->RSSetViewports(1, &vp);
}
void D3D::swap_buffers()
{
	TwDraw();
	swap_chain->Present(0, 0);	
}

void D3D::window_resized(const Window * window)
{
	if(device)
	{
		// Release render target and depth-stencil view
        ID3D11RenderTargetView * null_rtv = NULL;
		immediate_ctx->OMSetRenderTargets(1, &null_rtv, NULL);
		if (back_buffer_rtv)
        {
			//NOTE: not calling release on IUnknown, but CComPtr
            back_buffer_rtv.Release();
            back_buffer_rtv = nullptr;
        }
        if (dsv)
        {
            dsv.Release();
			depth_srv.Release();
            dsv = nullptr;
        }

		if (swap_chain)
        {
            // Resize swap chain
			swap_chain_desc.BufferDesc.Width = window->size().cx;
            swap_chain_desc.BufferDesc.Height = window->size().cy;
			swap_chain->ResizeBuffers(swap_chain_desc.BufferCount, swap_chain_desc.BufferDesc.Width, 
                                        swap_chain_desc.BufferDesc.Height, swap_chain_desc.BufferDesc.Format, 
                                        swap_chain_desc.Flags);

            // Re-create a render target and depth-stencil view
            ID3D11Texture2D * back_buffer = nullptr;
			ID3D11Texture2D * depth_buffer = nullptr;

			swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&back_buffer);
			d3d::name(back_buffer, "backbuffer after resize");
			device->CreateRenderTargetView(back_buffer, NULL, &back_buffer_rtv);
			d3d::name(back_buffer_rtv.p, "backbuffer after resize rtv");
            back_buffer->Release();
			depth_stencil_desc.Width = swap_chain_desc.BufferDesc.Width;
            depth_stencil_desc.Height = swap_chain_desc.BufferDesc.Height;
			device->CreateTexture2D(&depth_stencil_desc, NULL, &depth_buffer);
			d3d::name(depth_buffer, "depth after resize");
			auto depth_dsv_desc = make_depth_dsv_desc();
			auto depth_srv_desc = make_depth_srv_desc();

			device->CreateDepthStencilView(depth_buffer, &depth_dsv_desc, &dsv.p);	;
			d3d::name(dsv.p, "depth after resize dsv");		
			device->CreateShaderResourceView(depth_buffer, &depth_srv_desc, &depth_srv.p);
			d3d::name(depth_srv.p, "depth after resize srv");		
            depth_buffer->Release();

			set_viewport(window->size());
        }

	}
}
void D3D::draw(const d3d::DrawOp & draw_op)
{
	unsigned int offset = 0;
	immediate_ctx->IASetInputLayout(draw_op.il);
	immediate_ctx->IASetVertexBuffers(0, 1, &draw_op.vb.p, &draw_op.vb_stride, &offset);
	immediate_ctx->IASetIndexBuffer(draw_op.ib, DXGI_FORMAT_R32_UINT, 0);
	immediate_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	immediate_ctx->DrawIndexed(draw_op.ib_count, 0, 0);
}

void D3D::create_shaders_and_il(const wchar_t * file, 
	ID3D11VertexShader** vs, 
	ID3D11PixelShader** ps,
	ID3D11GeometryShader ** gs,
	ID3D11InputLayout** il ,
	gfx::VertexTypes type)
{
	auto vs_blob = d3d::load_shader(file, "vs", "vs_5_0");	
	auto l = vs_blob->GetBufferSize();
	device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, vs);
	
	d3d::name(*vs, file);
	auto ps_blob = d3d::load_shader(file, "ps", "ps_5_0");
	device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, ps);
	
	d3d::name(*ps, file);
	ps_blob->Release();
	if(gs != nullptr)
	{
		auto gs_blob = d3d::load_shader(file, "gs", "gs_4_0");
		device->CreateGeometryShader(gs_blob->GetBufferPointer(), gs_blob->GetBufferSize(), nullptr, gs);
		gs_blob->Release();
	
		d3d::name(*gs, file);
	}
	if(il != nullptr)
	{
		assert(type != gfx::eUnknown);
		if(type == gfx::eBasic || type == gfx::eFSQuad)
		{
			D3D11_INPUT_ELEMENT_DESC desc[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			device->CreateInputLayout(desc, 1, vs_blob->GetBufferPointer(),
				vs_blob->GetBufferSize(), il); 			
		}
		else if(type == gfx::eStandard)
		{
			D3D11_INPUT_ELEMENT_DESC desc[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12 , D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24 , D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			device->CreateInputLayout(desc, sizeof(desc) / sizeof(D3D11_INPUT_ELEMENT_DESC), vs_blob->GetBufferPointer(),
				vs_blob->GetBufferSize(), il); 
		}
		else if(type == gfx::eAnimatedStandard)
		{
			D3D11_INPUT_ELEMENT_DESC desc[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12 , D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24 , D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "BONES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 32 , D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48 , D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			device->CreateInputLayout(desc, sizeof(desc) / sizeof(D3D11_INPUT_ELEMENT_DESC), vs_blob->GetBufferPointer(),
				vs_blob->GetBufferSize(), il); 
		}
		else
		{
			assert(false);
		}
		d3d::name(*il, file);
	}
	vs_blob->Release();
}
namespace d3d
{		
	ID3D10Blob* load_shader(const wchar_t * file,
		const char * entry, const char * profile)
	{
		ID3D10Blob * shader_bin;
		ID3D10Blob * error_bin;
		auto hr = D3DX11CompileFromFile(file, nullptr, nullptr, entry, profile, 0
			//D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG 
			//| D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_PREFER_FLOW_CONTROL
			, 0, 0, &shader_bin, &error_bin, nullptr);
		if((error_bin != nullptr) || FAILED(hr))
		{
			const char * msg = nullptr;
			if(error_bin != nullptr)
			{
				msg = (const char *) error_bin->GetBufferPointer();
			}
			assert(false);
		}
		
		return shader_bin;
	}
}