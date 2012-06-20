
struct Effect
{
	void initialize(D3D& d3d, const wchar_t* path, gfx::VertexTypes vertexType)
	{				
		d3d.create_shaders_and_il(path, &vs.p, &ps.p, nullptr, &il.p, 
			vertexType);
	}
	void apply(D3D& d3d)
	{
		d3d.immediate_ctx->VSSetShader(vs, nullptr, 0);
		d3d.immediate_ctx->PSSetShader(ps, nullptr, 0);
		d3d.immediate_ctx->IASetInputLayout(il);
	}
	CComPtr<ID3D11VertexShader> vs;
	CComPtr<ID3D11PixelShader> ps;
	CComPtr<ID3D11InputLayout> il;
};