#include "stdafx.h"
#include "package.h"
#include <IL/il.h>

#include "gfx.h"

using namespace std;
namespace package
{
	const int PACKAGE_VERSION = 1;
	enum PackageContentType : int
	{
		eEnd = 0,
		eMesh,
		eMaterial,
		eTexture,
		eAnimation,
	};
	void write_block(ofstream& out, char* data, int byte_size)
	{
		bwrite(out, byte_size);
		out.write(data, byte_size);
	}
	void read_block(ifstream& in, char** data, int* byte_size)
	{
		bread(in, byte_size);
		*data = (char*)malloc(*byte_size);
		in.read(*data, *byte_size);
	}
	void read_mesh(ifstream& in, 
		Mesh* mesh)
	{
		//already read eMesh;
		//vert count
		bread(in, &mesh->vertex_count);
		int vertices_byte_size;
		//vert data
		read_block(in, &mesh->verticies, &vertices_byte_size);
		//compute vert stride
		mesh->vertex_stride = vertices_byte_size / mesh->vertex_count;
		int indices_byte_size;
		//indices
		read_block(in, (char**)&mesh->indices, &indices_byte_size);
		//compute # of indices
		mesh->indices_count = indices_byte_size / sizeof(GfxIndex);
		int animations_count;
		bread(in, &animations_count);
		for(int i = 0; i < animations_count; i++)
		{
			PackageHandle animation;
			bread(in, &animation);
			mesh->skeletal_animations.push_back(animation);
		}
		int parts_count;
		bread(in, &parts_count);
		for(int i = 0; i < parts_count; i++)
		{
			MeshPart part;
			bread(in, &part.indices_count);
			bread(in, &part.indices_offset);
			bread(in, &part.material);
			mesh->mesh_parts.push_back(part);
		}
	}
	void write_mesh(ofstream& out, 
		const gfx::Model* model,
		int written_animations_count,
		int written_materials_count)
	{
		bwrite(out, eMesh);
		//vert count
		bwrite(out, model->vertices_count);
		//vert data
		write_block(out, model->vertices, model->vertices_count * model->vertices_stride);
		//indexes
		write_block(out, (char*)model->indexes->ptr, model->indexes->byte_size);
		bwrite(out, model->animations.size());	
		for(int i = 0; i < model->animations.size(); i++)
		{
			bwrite(out, written_animations_count + i);
		}
		bwrite(out, model->parts.size());
		for(int i = 0; i < model->parts.size(); i++)
		{
			auto part = model->parts[i];
			bwrite(out, part.indices_count);
			bwrite(out, part.indices_offset);
			bwrite(out, written_materials_count + i);				
		}
	}
	
	void read_material(ifstream& in, 
		Material* material)
	{
		bread(in, &material->type);
		int textures_count;
		bread(in, &textures_count);
		for(int i = 0; i < textures_count; i++)
		{
			PackageHandle texture;
			bread(in, &texture);
			material->textures.push_back(texture);
		}
	}
	void write_material(ofstream& out,
		int written_textures_count, PackageHandle texture_handle)
	{
		bwrite(out, eMaterial);
		bwrite(out, 0); // MATERIAL TYPE.
		if(texture_handle > -1)
		{
			bwrite(out, 1); // # of textures
			bwrite(out, texture_handle);
		}
		else
		{
			bwrite(out, 0); // # of textures			
		}
	}
	void read_texture(ifstream& in, 
		Texture* texture)
	{
		bread(in, &texture->type);
		bread(in, &texture->width);
		bread(in, &texture->height);

		read_block(in, &texture->data, &texture->byte_size);
	}
	void write_texture(ofstream& out,
		const wchar_t* file)
	{
		bwrite(out, eTexture);
		bwrite(out, 0); // TEXTURE TYPE
		//get the albedo
		auto tex_id = ilGenImage();
		ilLoadImage(file);
		int byte_size = ilSaveL(IL_DDS, nullptr, 0);
				
		bwrite(out, ilGetInteger(IL_IMAGE_WIDTH));
		bwrite(out, ilGetInteger(IL_IMAGE_HEIGHT));
				
		char* buffer = (char*)malloc(byte_size);
		ilSaveL(IL_DDS, buffer, byte_size);
		ilDeleteImage(tex_id);		
		write_block(out, buffer, byte_size);
	}
	void read_animation(ifstream& in, SkeletalAnimation* animation)
	{
		bread(in, &animation->frames_count);
		bread(in, &animation->joints_count);
		int byte_size = sizeof(m44) * animation->frames_count * animation->joints_count;
		animation->data = (m44*)malloc(byte_size);
		in.read((char*)animation->data, byte_size);
	}
	void write_animation(ofstream& out, const gfx::Animation* animation)
	{
		int joints_count = animation->frames[0].bone_transforms->count;
		bwrite(out, eAnimation);
		bwrite(out, animation->frames.size());
		bwrite(out, animation->frames[0].bone_transforms->count);
		for(int i = 0; i < animation->frames.size(); i++)
		{					
			assert(animation->frames[i].bone_transforms->byte_size == 
				(joints_count * sizeof(m44)));
			int byte_size = animation->frames[i].bone_transforms->byte_size;
			out.write((char*)animation->frames[i].bone_transforms->ptr, 
				animation->frames[i].bone_transforms->byte_size);
		}
	}
	void load_package(const wchar_t* file, Package* package)
	{		
		ifstream in(file, ios::in | ios::binary);
		bread(in, &package->version);
		assert(in.good());
		while(!in.eof())
		{
			int content_type;
			bread(in, &content_type);
			switch(content_type)
			{
			case PackageContentType::eAnimation:
				{
					SkeletalAnimation animation;
					read_animation(in, &animation);
					bool good = in.good();
					bool eof = in.eof();
					package->skeletal_animations.push_back(animation);
					break;
				}
			case PackageContentType::eMaterial:
				{
					Material material;
					read_material(in, &material);
					assert(in.good());
					package->materials.push_back(material);
					break;
				}
			case PackageContentType::eMesh:
				{
					Mesh mesh;
					read_mesh(in, &mesh);
					assert(in.good());
					package->meshes.push_back(mesh);
					break;
				}
			case PackageContentType::eTexture:
				{
					Texture texture;
					read_texture(in, &texture);
					assert(in.good());
					package->textures.push_back(texture);
					break;
				}
			case PackageContentType::eEnd:
				{					
					break;
				}
			}
		}
		in.close();
	}
	void bake_package(const wchar_t* file, 
		const std::list<const gfx::Model*>* models)
	{
		
		ofstream out(file, ios::out | ios::binary | ios::trunc);
		bwrite(out, PACKAGE_VERSION); //version
		assert(out.good());
		int written_materials_count = 0;
		int written_textures_count = 0;
		int written_animations_count = 0;
		ilInit();

		//for(auto model : *models)
		for(auto it = models->begin(); it != models->end(); it++)
		{
			auto model = *it;
			write_mesh(out, model, written_animations_count, written_materials_count);
			assert(out.good());
			//for(auto part : model->parts)
			for(auto parts_it = model->parts.begin(); parts_it != model->parts.end(); parts_it++)
			{
				auto part = *parts_it;
				bool has_texture = part.albedo.size() > 0;
				write_material(out, written_materials_count, 
					(has_texture ? written_textures_count: -1));
				written_materials_count++;
				assert(out.good());
				if(has_texture)
				{
					write_texture(out, part.albedo.c_str());
					written_textures_count++;
				}
				assert(out.good());

			}
			//for(auto animation : model->animations)
			for(auto it = model->animations.begin(); it != model->animations.end(); it++)
			{
				auto animation = *it;
				write_animation(out, animation.get());	
				assert(out.good());
				written_animations_count ++;
			}
		}
		assert(out.good());
		bwrite(out, eEnd);
		out.close();
	}
	
}