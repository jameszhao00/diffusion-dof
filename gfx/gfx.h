#pragma once
#include <memory>
#include <hash_map>
#include "lwmath.h"

using namespace std;
struct Ray
{
	float origin[3];
	float direction[3];
};
typedef ID3D11VertexShader VertexShader;
typedef ID3D11PixelShader PixelShader;
typedef ID3D11GeometryShader GeometryShader;
typedef ID3D11ComputeShader ComputeShader;
typedef ID3D11Buffer Uniforms;
namespace gfx
{
	const unsigned int BONES_PER_VERTEX = 4;
	enum VertexTypes
	{
		eUnknown,
		eBasic,
		eStandard,
		eAnimatedStandard,
		eFSQuad
	};
	
	struct StandardVertex
	{
		StandardVertex()
		{
			ZeroMemory(this, sizeof(*this));		
		}
		float position[3];
		float normal[3];
		float uv[2];
	};
	struct StandardAnimatedVertex
	{		
		StandardAnimatedVertex()
		{
			ZeroMemory(this, sizeof(*this));
		}
		float position[3];
		float normal[3];
		float uv[2];

		unsigned int bones[BONES_PER_VERTEX];
		float bone_weights[BONES_PER_VERTEX];
	};
	struct BasicVertex
	{
		float position[4];
	};
	template<typename T>
	struct Data
	{	
		Data(int num_elements) : count(num_elements)
		{
			ptr = new T[num_elements] ;
			byte_size = num_elements * sizeof(T);
			stride = sizeof(T);
		}
		~Data()
		{
			delete [] ptr;
		}
		T* ptr;
		unsigned int count;
		unsigned int byte_size;
		unsigned int stride;
	};	
	
	struct AnimationFrame
	{
		vector<float4x4> bone_transforms;
		int byteSize() const { return sizeof(float4x4) * bone_transforms.size(); }
	};
	struct Animation
	{
		vector<AnimationFrame> frames;
		string name;
	};
	
	struct MeshPart
	{
		wstring albedo;
		int indices_count;
		int indices_offset;
	};
	struct Model
	{
		string id;

		char * vertices;
		int vertices_count;
		int vertices_stride;
		shared_ptr<Data<unsigned int>> indexes;
		list<shared_ptr<Animation>> animations;
		

		float center[3];
		
		vector<MeshPart> parts;

	};
};

typedef vector<gfx::StandardAnimatedVertex> VertexCollection;
typedef vector<unsigned int> IndexCollection;
typedef vector<char> Texture;
typedef vector<unique_ptr<Texture>> TextureCollection;
struct RuntimeMeshPart
{
	int indexOffset;
	int indexCount;
	vector<int> textures;
};
struct RuntimeMesh
{
	enum Type
	{
		eStatic,
		eAnimated
	};
	Type type;
	/*
	struct
	{
		VertexCollection vertices;
		IndexCollection indices;
		TextureCollection textures;
	} build;
	*/
	vector<RuntimeMeshPart> parts;
	struct
	{
		CComPtr<ID3D11Buffer> vb;
		CComPtr<ID3D11Buffer> ib;
		vector<CComPtr<ID3D11ShaderResourceView>> textures;
		unsigned int vertexStride;
	} live;
};