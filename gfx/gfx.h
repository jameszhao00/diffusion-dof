#pragma once
#include <DirectXMath.h>
#include <memory>
#include <hash_map>

using namespace std;
struct Ray
{
	float origin[3];
	float direction[3];
};
typedef ID3D11VertexShader VertexShader;
typedef ID3D11PixelShader PixelShader;
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
	
	struct float4x4 { float data[4][4]; };

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
		shared_ptr<Data<float4x4>> bone_transforms;
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