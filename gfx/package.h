#pragma once
#include <list>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
namespace gfx
{
	struct Model;
};
namespace package
{
	const int TEXTURES_PER_MATERIAL = 8;
	const int SKELETAL_ANIMATIONS_PER_MESH = 8;
	typedef float m44[4][4];
	typedef int PackageHandle;
	typedef unsigned int GfxIndex;
	enum MeshType
	{
		eStatic,
		eAnimated
	};
	struct MeshPart
	{
		int indices_offset; //into the Mesh.indices array
		int indices_count; //# of indices to use in the indices array
		PackageHandle material;
	};
	struct Mesh
	{
		int type; 
		int vertex_stride;
		int vertex_count;
		char* verticies;
		int indices_count;
		GfxIndex* indices;
		std::vector<MeshPart> mesh_parts;
		std::vector<PackageHandle> skeletal_animations;
	};
	struct Material
	{
		int type;
		std::vector<PackageHandle> textures;
	};
	struct Texture
	{
		int type;
		int width;
		int height;
		int byte_size;
		char* data;
	};
	struct SkeletalAnimation
	{
		int frames_count;
		int joints_count;
		m44* data; //m44 count = frames_count * joints_count 
	};
	struct Package
	{
		int version;
		std::vector<Mesh> meshes;
		std::vector<Material> materials;
		std::vector<Texture> textures;
		std::vector<SkeletalAnimation> skeletal_animations;
	};
	
	void bake_package(const wchar_t * file, 
		const std::list<const gfx::Model*>* models);
	
	void load_package(const wchar_t* file, Package* package);

	
	template<typename T> 
	void bwrite(std::ofstream& out, const T& value)
	{
		out.write((char*)&value, sizeof(T));
	}
	template<typename T> 
	void bread(std::ifstream& in, T* value)
	{
		in.read((char*)value, sizeof(T));
	}
}