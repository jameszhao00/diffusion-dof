#include "stdafx.h"
#include "fbx.h"
#include "gfx.h"

#include <list>
#include <algorithm>

#include <assimp\assimp.h>
#include <assimp\aiScene.h>
#include <assimp\aiPostProcess.h>

//#include <DirectXMath.h>
#include <xnamath.h>

#include <fbxsdk.h>

//using namespace DirectX;
using namespace std;
using namespace gfx;
namespace asset
{	

	/*
	template<typename T>
	struct Bone
	{
		aiBone * bone;
		aiNode * node;
		list<Bone> children;
	};
	shared_ptr<Data<unsigned int>> mesh_2_ib(const aiMesh * mesh)
	{
		auto index_data = make_shared<Data<unsigned int>>(mesh->mNumFaces * 3);
		for(unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			assert(mesh->mFaces[i].mNumIndices == 3);
			for(int j = 0; j < 3; j++) index_data->ptr[i * 3 + j] = mesh->mFaces[i].mIndices[j];
		}
		return index_data;
	}
	const aiNode * find_skeleton_root(unsigned int mesh_index, const aiNode * cur)
	{
		for(int i = 0; i < cur->mNumMeshes; i++)
		{
			if(cur->mMeshes[i] == mesh_index) return cur;			
		}
		
		return find_skeleton_root(mesh_index, cur->mParent);
	}
	const aiNode * find_skeleton_root(unsigned int mesh_index, const aiScene * scene)
	{
		return find_skeleton_root(mesh_index, scene->mRootNode->FindNode(scene->mMeshes[mesh_index]->mBones[0]->mName));
	}
	
	bool node_references_mesh(const aiNode * node, unsigned int mesh_id)
	{
		for(int i = 0; i < node->mNumMeshes; i++) 
		{
			if(node->mMeshes[i] == mesh_id) return true;
		}
		return false;
	}

	const aiNode * find_skeleton_root(const aiNode * cur, unsigned int mesh_id)
	{
		if(cur == nullptr) assert(false); //we never found the mesh...
		
		if(node_references_mesh(cur->mParent, mesh_id)) return cur;
		
		for(int i = 0; i < cur->mParent->mNumChildren; i++)
		{
			if(node_references_mesh(cur->mParent->mChildren[i], mesh_id)) return cur;
		}

		return find_skeleton_root(cur->mParent, mesh_id);
	}

	const aiNode * find_skeleton_root(const aiMesh * mesh, unsigned int mesh_id, const aiScene * scene)
	{
		auto node = scene->mRootNode->FindNode(mesh->mBones[0]->mName);

		return find_skeleton_root(node, mesh_id);
	}
	shared_ptr<hash_map<const aiNode*, vector<aiMatrix4x4>>> build_node_to_root_frames(
		hash_map<const aiNode*, vector<aiMatrix4x4>> & node_to_parent_frames,
		const aiNode * root)
	{
		auto node_to_root_frames = 
			make_shared<hash_map<const aiNode*, vector<aiMatrix4x4>>>();
		for(auto it = node_to_parent_frames.begin(); it != node_to_parent_frames.end(); it++)
		{
			auto next = it->first->mParent;
			auto current_frames = it->second;
			while(next != root->mParent)
			{
				//the parent may not have any animation frames....
				if(node_to_parent_frames.find(next) == node_to_parent_frames.end())
				{
					for(int frame_i = 0; frame_i < current_frames.size(); frame_i++)
					{
						current_frames[frame_i] = current_frames[frame_i] * next->mTransformation;
					}
				}
				else
				{
					auto next_frames = node_to_parent_frames[next];
					for(int frame_i = 0; frame_i < current_frames.size(); frame_i++)
					{
						current_frames[frame_i] = current_frames[frame_i] * next_frames[frame_i];
					}
				}
				next = next->mParent;
			}
			(*node_to_root_frames)[it->first] = current_frames;
		}
		return node_to_root_frames;
	}
	
	vector<aiMatrix4x4> build_node_to_parent_frames(const aiNodeAnim * channel)
	{
		vector<aiMatrix4x4> node_to_parent_frames;
				
		
		//restriction for now
		assert(channel->mNumPositionKeys == channel->mNumRotationKeys);
		assert(channel->mNumPositionKeys == channel->mNumScalingKeys);
		
		for(auto frame_i = 0; frame_i < channel->mNumPositionKeys; frame_i++)
		{
			aiMatrix4x4 scale;
			aiMatrix4x4 rotation = aiMatrix4x4(channel->mRotationKeys[frame_i].mValue.GetMatrix());				
			aiMatrix4x4 translation;
			aiMatrix4x4::Scaling(channel->mScalingKeys[frame_i].mValue, scale);
			aiMatrix4x4::Translation(channel->mPositionKeys[frame_i].mValue, translation);

			node_to_parent_frames.push_back(translation * rotation * scale);
		}
		return node_to_parent_frames;
	}
	aiNode * get_skeletal_node(const aiNode * mesh_node)
	{
		bool has_skeletal_sibling = mesh_node->mParent != nullptr && mesh_node->mParent->mNumChildren > 1;
		if(has_skeletal_sibling) assert(mesh_node->mParent->mNumChildren == 2);
		else assert(mesh_node->mNumChildren == 1);

		aiNode * skeletal_node = nullptr;
		if(has_skeletal_sibling)
		{
			for(int i = 0; i < mesh_node->mParent->mNumChildren; i++)
			{
				if(mesh_node->mParent->mChildren[i] != mesh_node) 
				{
					skeletal_node = mesh_node->mParent->mChildren[i];
					break;
				}
			}
			assert(skeletal_node != nullptr);
		}
		else
		{
			skeletal_node = mesh_node->mChildren[0];
		}
		return skeletal_node;
	}
	aiNode * find_mesh_node(aiNode * node)
	{
		if(node->mNumMeshes == 1) return node;
		for(int i = 0; i < node->mNumChildren; i++)
		{
			auto child_result = find_mesh_node(node->mChildren[i]);
			if(child_result != nullptr) return child_result;
		}
		return nullptr;
	}
	struct BoneAnimPeer
	{
		aiNode * node;
		list<shared_ptr<BoneAnimPeer>> children;
		vector<aiMatrix4x4> parent_frames;
		vector<aiMatrix4x4> global_frames;
		void build_global_frames(int frame_n)
		{
			if(parent_frames.empty())
			{
				aiMatrix4x4 identity; aiIdentityMatrix4(&identity);
				parent_frames.resize(frame_n, identity);
			}
			else if(parent_frames.size() == 1)
			{
				parent_frames.resize(frame_n, parent_frames.front());
			}
			
			aiMatrix4x4 identity; aiIdentityMatrix4(&identity);
			vector<aiMatrix4x4> parent_global_frames;
			parent_global_frames.resize(frame_n, identity);
			build_global_frames(parent_global_frames);
		}
		void build_global_frames(vector<aiMatrix4x4> & parent_global_frames)
		{
			assert(parent_global_frames.size() == parent_frames.size());
			
			for(int i = 0; i < parent_global_frames.size(); i++)
			{
				global_frames.push_back(parent_frames[i] * parent_global_frames[i]);
			}
			for(auto child : children)
			{
				child->build_global_frames(global_frames);
			}
		}
	};
	shared_ptr<BoneAnimPeer> node_to_peer(aiNode * node)
	{
		auto peer = make_shared<BoneAnimPeer>();
		peer->node = node;
		for(int i = 0; i < node->mNumChildren; i++)
		{
			peer->children.push_back(node_to_peer(node->mChildren[i]));
		}
		return peer;
	}
	void make_node_peers_map(shared_ptr<BoneAnimPeer> peer, hash_map<const aiNode*, shared_ptr<BoneAnimPeer>> & node_peers_map)
	{
		node_peers_map[peer->node] = peer;
		for(auto child : peer->children)
		{
			make_node_peers_map(child, node_peers_map);
		}
	}
	hash_map<const aiNode*, shared_ptr<BoneAnimPeer>> make_node_peers_map(shared_ptr<BoneAnimPeer> peer)
	{
		hash_map<const aiNode*, shared_ptr<BoneAnimPeer>> node_peers_map;
		make_node_peers_map(peer, node_peers_map);
		return node_peers_map;
	}
	hash_map<aiNode *, unsigned int> make_skeletal_node_to_index_map(aiMesh * mesh, const aiScene * scene)
	{
		hash_map<aiNode *, unsigned int> result;
		for(int i = 0; i < mesh->mNumBones; i++)
		{
			auto node = scene->mRootNode->FindNode(mesh->mBones[i]->mName);
			result[node] = i;
		}
		return result;
	}
	void make_peer_list(shared_ptr<BoneAnimPeer> peer, hash_map<aiNode *, unsigned int> & ordering,
		vector<shared_ptr<BoneAnimPeer>> & peers)
	{
		assert(peers.size() == ordering.size());
		peers[ordering[peer->node]] = peer;
		for(auto child : peer->children)
		{
			make_peer_list(child, ordering, peers);
		}
	}
	vector<shared_ptr<BoneAnimPeer>> make_peer_list(shared_ptr<BoneAnimPeer> peer, hash_map<aiNode *, unsigned int> & ordering)
	{		
		vector<shared_ptr<BoneAnimPeer>> peers;
		peers.resize(ordering.size());
		make_peer_list(peer, ordering, peers);
		return peers;
	}
#include "d3d.h"
	list<shared_ptr<Animation>> make_animations(aiMesh * mesh, aiNode * node, const aiScene * scene)
	{
		assert(node->mNumMeshes == 1);
		list<shared_ptr<Animation>> animations;
		auto skeletal_node = get_skeletal_node(node);

		for(int anim_i = 0; anim_i < scene->mNumAnimations; anim_i++)
		{			
			//we don't have to recreate the following for every anim..
			auto root_peer = node_to_peer(skeletal_node);
			auto node_peers_map = make_node_peers_map(root_peer);
			auto anim = scene->mAnimations[anim_i];
			for(int channel_i = 0; channel_i < anim->mNumChannels; channel_i++)
			{
				auto channel = anim->mChannels[channel_i];

				auto peer = node_peers_map[node->FindNode(channel->mNodeName)];
				peer->parent_frames = build_node_to_parent_frames(channel);
			}

			int frame_n = anim->mChannels[0]->mNumPositionKeys;
			root_peer->build_global_frames(frame_n);

			auto skeletal_node_to_index = make_skeletal_node_to_index_map(mesh, scene);
			auto peer_list = make_peer_list(root_peer, skeletal_node_to_index);

			auto gfx_anim = make_shared<Animation>();
			assert(peer_list.size() == mesh->mNumBones);
			for(int frame_i = 0; frame_i < frame_n; frame_i++)
			{
				AnimationFrame frame;
				frame.bone_transforms = make_shared<Data<float4x4>>(MAX_BONES);
				for(int peer_i = 0; peer_i < peer_list.size(); peer_i++)
				{
					auto global = peer_list[peer_i]->global_frames[frame_i];
					auto offset = mesh->mBones[peer_i]->mOffsetMatrix;
					auto mat =  global * offset;

					assert(sizeof(mat) == sizeof(float4x4));
					memcpy(&frame.bone_transforms->ptr[peer_i], &mat, sizeof(mat));
				}
				gfx_anim->frames.push_back(frame);
			}
			animations.push_back(gfx_anim);			 
		}

		return animations;
	}

	shared_ptr<Model<StandardAnimatedVertex>> load_animated_model(const char * path)
	{
		auto scene = aiImportFile(path, aiProcess_CalcTangentSpace       | 
			aiProcess_Triangulate            |
			aiProcess_JoinIdenticalVertices  |
		//	aiProcess_MakeLeftHanded | 
			aiProcess_SortByPType);
		
		auto model = make_shared<Model<StandardAnimatedVertex>>();
		
		assert(scene);

		assert(scene->HasMeshes());
		//for(unsigned int mesh_i = 0; mesh_i < scene->mNumMeshes; mesh_i++)
		{
			//only allow 1 mesh for animated models
			unsigned int mesh_i = 0;

			auto mesh = scene->mMeshes[mesh_i];
			

			float min_x = 1000000000, min_y = 1000000000, min_z = 1000000000, 
			max_x = -1000000000, max_y = -1000000000, max_z = -1000000000;

			auto vert_data = make_shared<Data<StandardAnimatedVertex>>(mesh->mNumVertices);
					
			auto skeleton_root = find_skeleton_root(mesh, mesh_i, scene);

			for(unsigned int i = 0; i < mesh->mNumVertices; i++)
			{
				min_x = min(mesh->mVertices[i].x, min_x);
				max_x = max(mesh->mVertices[i].x, max_x);

				min_y = min(mesh->mVertices[i].y, min_y);
				max_y = max(mesh->mVertices[i].y, max_y);

				min_z = min(mesh->mVertices[i].z, min_z);
				max_z = max(mesh->mVertices[i].z, max_z);

				vert_data->ptr[i].position[0] = mesh->mVertices[i].x;
				vert_data->ptr[i].position[1] = mesh->mVertices[i].y;
				vert_data->ptr[i].position[2] = mesh->mVertices[i].z;

				vert_data->ptr[i].normal[0] = mesh->mNormals[i].x;
				vert_data->ptr[i].normal[1] = mesh->mNormals[i].y;
				vert_data->ptr[i].normal[2] = mesh->mNormals[i].z;
			

				vert_data->ptr[i].uv[0] = 7;
				vert_data->ptr[i].uv[1] = 8;
			}
			unsigned int * bones_used_per_vert = new unsigned int[mesh->mNumVertices];
			ZeroMemory(bones_used_per_vert, sizeof(unsigned int) * mesh->mNumVertices);



			for(unsigned int bone_i = 0; bone_i < mesh->mNumBones; bone_i++)
			{
				auto bone = mesh->mBones[bone_i];
				
				for(unsigned int weight_i = 0; weight_i < bone->mNumWeights; weight_i++)
				{
					auto weight = bone->mWeights[weight_i];
					auto vert_bone_i = bones_used_per_vert[weight.mVertexId];

					assert(vert_bone_i < 4);
					
					vert_data->ptr[weight.mVertexId].bones[vert_bone_i] = bone_i;
					vert_data->ptr[weight.mVertexId].bone_weights[vert_bone_i] = weight.mWeight;
					//can we increment like this?
					bones_used_per_vert[weight.mVertexId]++;
				}
			}
			delete [] bones_used_per_vert;
			
			auto index_data = mesh_2_ib(mesh);
		

			model->center[0] = (max_x + min_x) / 2;
			model->center[1] = (max_y + min_y) / 2;
			model->center[2] = (max_z + min_z) / 2;

			model->vertices = vert_data;
			model->indexes = index_data;
			model->id = string(mesh->mName.data);
			
			model->animations = make_animations(scene->mMeshes[0], scene->mRootNode, scene);

		}

		aiReleaseImport(scene);
		return model;
	}
	*/
	namespace fbx
	{
		
		//each cp can have more than 1 material

		//polyvert index -> material

		//cp -> material
		//cp -> {material, material, ...}
		//cp -> {mesh0's cp, mesh1's cp, ...}

		//for each joint
			//for each cp
				//for each material cp has
					//look up cp's new index in the submesh
					//set bone/weight in submesh

		/*
		void fill_bone_data_for_splitted_meshes(
			KFbxMesh * original_mesh, 
			vector<shared_ptr<Model<StandardAnimatedVertex>>> material_to_model_map,
			shared_ptr<vector<vector<int>>> cp_to_materials_map, 
			shared_ptr<vector<vector<int>>> cp_to_material_cps_map)
		{
		
			auto skin_count = original_mesh->GetDeformerCount(KFbxDeformer::eSKIN);
			assert(skin_count == 1);
			auto skin = (KFbxSkin*) original_mesh->GetDeformer(0, KFbxDeformer::eSKIN);

			auto cluster_count = skin->GetClusterCount();
		
			//in the original mesh's space
			int * vert_influences_count = new int[original_mesh->GetControlPointsCount()];
			memset(vert_influences_count, 0, sizeof(int) * original_mesh->GetControlPointsCount());
			
			//for each joint
			for(int cluster_i = 0; cluster_i < cluster_count; cluster_i++)
			{
				auto cluster = skin->GetCluster(cluster_i);

				auto indices = cluster->GetControlPointIndices();
				auto weights = cluster->GetControlPointWeights();
				auto k = cluster->GetControlPointIndicesCount();
				assert(cluster->GetLinkMode() == KFbxCluster::ELinkMode::eNORMALIZE);
				//assert(vert_count >= cluster->GetControlPointIndicesCount());
				
				//for each cp
				for(int indice_i = 0; indice_i < cluster->GetControlPointIndicesCount(); indice_i++)				
				{									
					auto original_cp = indices[indice_i];
					auto bone_index = vert_influences_count[original_cp];
					
					//for each material cp has
					for(int material_i = 0; material_i < cp_to_materials_map->at(original_cp).size(); material_i++)
					{
						auto material = cp_to_materials_map->at(original_cp).at(material_i);
						
						//look up cp's new index in the submesh
						auto new_cp = cp_to_material_cps_map->at(original_cp).at(material_i);
						auto new_mesh = material_to_model_map[material];
						
						//set bone/weight in submesh
						new_mesh->vertices->ptr[new_cp].bones[bone_index] = cluster_i;
						new_mesh->vertices->ptr[new_cp].bone_weights[bone_index] = weights[indice_i];
						vert_influences_count[original_cp]++;
						
						assert(vert_influences_count[original_cp] < 5);
					}
				}
			}
			delete [] vert_influences_count;

			for(auto model : material_to_model_map)
			{
				normalize_bone_weights(model);
			}
		}
		shared_ptr<Model<StandardAnimatedVertex>> import_mesh(KFbxMesh * mesh)
		{
			assert(mesh->IsTriangleMesh());
	

			shared_ptr<Model<StandardAnimatedVertex>> model(new Model<StandardAnimatedVertex>());

			auto vert_count = mesh->GetControlPointsCount();	
			model->vertices = make_shared<Data<StandardAnimatedVertex>>(vert_count);
			auto normals_element = mesh->GetLayer(0)->GetNormals();
			auto uvs_element = mesh->GetLayer(0)->GetUVs();

			if(normals_element) assert(normals_element->GetMappingMode() == KFbxLayerElement::eBY_CONTROL_POINT);
			if(uvs_element) assert(uvs_element->GetMappingMode() == KFbxLayerElement::eBY_CONTROL_POINT);

			for(auto vert_i = 0; vert_i < vert_count; vert_i++)
			{
				auto vert_pos = mesh->GetControlPointAt(vert_i);
				model->vertices->ptr[vert_i].position[0] = vert_pos.mData[0];
				model->vertices->ptr[vert_i].position[1] = vert_pos.mData[1];
				model->vertices->ptr[vert_i].position[2] = vert_pos.mData[2];

				if(normals_element)
				{
					if(normals_element->GetReferenceMode() == KFbxLayerElement::eDIRECT)
					{
						auto normal = normals_element->GetDirectArray().GetAt(vert_i);
						model->vertices->ptr[vert_i].normal[0] = normal[0];
						model->vertices->ptr[vert_i].normal[1] = normal[1];
						model->vertices->ptr[vert_i].normal[2] = normal[2];
					}
					else
					{					
						auto index = normals_element->GetIndexArray().GetAt(vert_i);
						auto normal = normals_element->GetDirectArray().GetAt(index);
						model->vertices->ptr[vert_i].normal[0] = normal[0];
						model->vertices->ptr[vert_i].normal[1] = normal[1];
						model->vertices->ptr[vert_i].normal[2] = normal[2];
					}
				}
				if(uvs_element)
				{
					if(uvs_element->GetReferenceMode() == KFbxLayerElement::eDIRECT)
					{					
						auto uv = uvs_element->GetDirectArray().GetAt(vert_i);
						model->vertices->ptr[vert_i].uv[0] = uv[0];
						model->vertices->ptr[vert_i].uv[1] = uv[1];

					}
					else
					{
						auto index = uvs_element->GetIndexArray().GetAt(vert_i);
						auto uv = uvs_element->GetDirectArray().GetAt(index);
						model->vertices->ptr[vert_i].uv[0] = uv[0];
						model->vertices->ptr[vert_i].uv[1] = uv[1];

					}
				}
			}

			auto index_count = mesh->GetPolygonVertexCount();
			model->indexes = make_shared<Data<unsigned int>>(index_count);
	
			for(auto index_i = 0; index_i < index_count; index_i++)
			{
				model->indexes->ptr[index_i] = (unsigned int)mesh->GetPolygonVertices()[index_i];
			}
		

			mesh->ComputeBBox();
			model->center[0] = (mesh->BBoxMax.Get().mData[0] + mesh->BBoxMin.Get().mData[0])/2;
			model->center[1] = (mesh->BBoxMax.Get().mData[1] + mesh->BBoxMin.Get().mData[1])/2;
			model->center[2] = (mesh->BBoxMax.Get().mData[2] + mesh->BBoxMin.Get().mData[2])/2;

			return model;
		}
		*/
		//NEED
		//doesn't need to worry about mesh splitting
		list<shared_ptr<Animation>> fill_animations(KFbxScene * scene, KFbxMesh * mesh)
		{		
			list<shared_ptr<Animation>> animations;

			KArrayTemplate<KString*> anim_stack_names;
			scene->FillAnimStackNameArray(anim_stack_names);
		
			auto skin_count = mesh->GetDeformerCount(KFbxDeformer::eSKIN);
			if(skin_count == 1)
			{
				auto skin = (KFbxSkin*) mesh->GetDeformer(0, KFbxDeformer::eSKIN);

				auto cluster_count = skin->GetClusterCount();

				for(int anim_stack_i = 0; anim_stack_i < anim_stack_names.GetCount(); anim_stack_i++)
				{			
					auto animation = make_shared<Animation>();

					auto anim_stack_name = anim_stack_names[anim_stack_i]->Buffer();
			
					animation->name = anim_stack_name;
					auto anim_stack = scene->FindMember((KFbxAnimStack*)0, anim_stack_name);
					//anim_stack->GetMember(FBX_TYPE(KFbxAnimLayer), 0);
					scene->GetEvaluator()->SetContext(anim_stack);
					auto take_info = scene->GetTakeInfo(anim_stack_name);

					auto start_time = take_info->mLocalTimeSpan.GetStart();
					auto end_time = take_info->mLocalTimeSpan.GetStop();
						
					KTime sampling_interval;
					sampling_interval.SetMilliSeconds(floor(1000.0 / 60));

					auto current_time = start_time;
					while(current_time < end_time)
					{
						AnimationFrame anim_frame;
						anim_frame.bone_transforms = make_shared<Data<float4x4>>(cluster_count);

						for(auto cluster_i = 0; cluster_i < cluster_count; cluster_i++)
						{					
							auto cluster = skin->GetCluster(cluster_i);

							assert(cluster->GetLinkMode() == KFbxCluster::ELinkMode::eNORMALIZE);
							assert(mesh->GetNodeCount() == 1); 

							KFbxXMatrix bind_model_2_world;
							KFbxXMatrix bind_joint_2_world;
		
							KFbxXMatrix anim_model_2_world;
							KFbxXMatrix anim_joint_2_world;
		
							KFbxXMatrix bind_model_2_joint;
							KFbxXMatrix anim_joint_2_model;

							KFbxXMatrix bind_model_2_anim_model;

							cluster->GetTransformMatrix(bind_model_2_world);
							anim_model_2_world = mesh->GetNode()->EvaluateGlobalTransform(current_time);
		
							cluster->GetTransformLinkMatrix(bind_joint_2_world);
							anim_joint_2_world = cluster->GetLink()->EvaluateGlobalTransform(current_time);

							bind_model_2_joint = bind_joint_2_world.Inverse() * bind_model_2_world;				
							anim_joint_2_model = anim_model_2_world.Inverse() * anim_joint_2_world;

							bind_model_2_anim_model = anim_joint_2_model * bind_model_2_joint;

							auto t_bind_model_2_anim_model = bind_model_2_anim_model.Transpose();

							for(int i = 0; i < 4; i++)
							{
								for(int j = 0; j < 4; j++)
								{
									anim_frame.bone_transforms->ptr[cluster_i].data[i][j] = (float)
										t_bind_model_2_anim_model.Double44()[i][j];
								}
							}

						}
						animation->frames.push_back(anim_frame);
				

						current_time += sampling_interval;
					}
					animations.push_back(animation);
				}
			}
			return animations;
		}
		/*
		shared_ptr<Model<StandardAnimatedVertex>> load_fbx_model(const char * path)
		{	
		 
			auto sdk_manager = KFbxSdkManager::Create();
			auto scene = KFbxScene::Create(sdk_manager, "");
			auto importer = KFbxImporter::Create(sdk_manager, "");
			auto error = importer->Initialize(path);
			auto result = importer->Import(scene);
			importer->Destroy();
		
			KFbxGeometryConverter geo_converter(sdk_manager);;


			auto mesh_node = find_mesh_node(scene->GetRootNode());
			assert(mesh_node != nullptr);
			assert(geo_converter.TriangulateInPlace(mesh_node));
			auto mesh = mesh_node->GetMesh();
			assert(mesh->SplitPoints(KFbxLayerElement::ELayerElementType::eNORMAL));
			//need 1 uv / vert
			assert(mesh->SplitPoints(KFbxLayerElement::ELayerElementType::eDIFFUSE_TEXTURES));

			assert(geo_converter.SplitMeshPerMaterial(mesh));		
			auto count = mesh_node->GetNodeAttributeCount();
			auto model = import_mesh(mesh);
			model->animations = fill_animations(scene, mesh);
			sdk_manager->Destroy();
			return model;

		}
		*/
	
		struct AssetVertex
		{
			float position[3];
			float normal[3];
			float uv[2];
			unsigned int joints[4];
			float joint_weights[4];
		};
		struct AssetTriangle
		{
			unsigned int material_index;
			unsigned int vertex_indices[3];
		};
		struct AssetModel
		{
			vector<AssetVertex> verticies;
			vector<AssetTriangle> triangles;
			vector<wstring> materials;
			vector<shared_ptr<AssetModel>> split_by_materials()
			{
				vector<shared_ptr<AssetModel>> models;
				for(auto material_i = 0; material_i < materials.size(); material_i++)
				{
					//maps from a vert index in the original vertices array
					//to a vert index in the new vertices array
					int * vert_i_to_part_vert_i = new int[verticies.size()];
					memset(vert_i_to_part_vert_i, -1, sizeof(int) * verticies.size());

					shared_ptr<AssetModel> model(new AssetModel());
					models.push_back(model);
					model->materials.push_back(materials[material_i]);
					//for(auto tri : triangles)
					for(auto it = triangles.begin(); it != triangles.end(); it++)
					{
						auto tri = *it;
						if(tri.material_index == material_i)
						{
							AssetTriangle new_tri;
							//the only material in the new model... so 0
							new_tri.material_index = 0;
							for(auto trivert_i = 0; trivert_i < 3; trivert_i++)
							{
								auto old_vert = tri.vertex_indices[trivert_i];
								auto new_vert = vert_i_to_part_vert_i[old_vert];
								if(new_vert == -1)
								{
									model->verticies.push_back(verticies.at(old_vert));
									new_vert = model->verticies.size() - 1;
									vert_i_to_part_vert_i[old_vert] = new_vert;
								}
								new_tri.vertex_indices[trivert_i] = new_vert;
							}
							model->triangles.push_back(new_tri);
						}
					}
				}
				return models;
			}
		};
		//NEED
		shared_ptr<AssetModel> mesh_to_asset_model(KFbxMesh * mesh)
		{
			assert(mesh->IsTriangleMesh());
	
			shared_ptr<AssetModel> model(new AssetModel());

			auto vert_count = mesh->GetControlPointsCount();	

			auto normals_element = mesh->GetLayer(0)->GetNormals();
			auto uvs_element = mesh->GetLayer(0)->GetUVs();

			if(normals_element) assert(normals_element->GetMappingMode() == KFbxLayerElement::eBY_CONTROL_POINT);
			if(uvs_element) assert(uvs_element->GetMappingMode() == KFbxLayerElement::eBY_CONTROL_POINT);

			for(auto vert_i = 0; vert_i < vert_count; vert_i++)
			{
				auto vert_pos = mesh->GetControlPointAt(vert_i);

				AssetVertex vert;
				vert.position[0] = vert_pos.mData[0];
				vert.position[1] = vert_pos.mData[1];
				vert.position[2] = vert_pos.mData[2];

				if(normals_element)
				{
					KFbxVector4 normal;
					if(normals_element->GetReferenceMode() == KFbxLayerElement::eDIRECT)
					{
						normal = normals_element->GetDirectArray().GetAt(vert_i);
					}
					else
					{					
						auto index = normals_element->GetIndexArray().GetAt(vert_i);
						normal = normals_element->GetDirectArray().GetAt(index);
					}
					vert.normal[0] = normal[0];
					vert.normal[1] = normal[1];
					vert.normal[2] = normal[2];
				}
				if(uvs_element)
				{
					KFbxVector2 uv;
					if(uvs_element->GetReferenceMode() == KFbxLayerElement::eDIRECT)
					{					
						uv = uvs_element->GetDirectArray().GetAt(vert_i);
					}
					else
					{
						auto index = uvs_element->GetIndexArray().GetAt(vert_i);
						uv = uvs_element->GetDirectArray().GetAt(index);
					}					
					vert.uv[0] = uv[0];
					//flip uv for fbx -> d3d
					vert.uv[1] = 1 - uv[1];
				}
				model->verticies.push_back(vert);
			}
			//triangles and materials
			assert(mesh->GetElementMaterialCount() == 1);
			auto material_element = mesh->GetElementMaterial();
			if(material_element)
			{
				//assert(material_element->GetMappingMode() == KFbxGeometryElement::eBY_POLYGON);
				//assert(material_element->GetIndexArray().GetCount() == mesh->GetPolygonCount());
			}
			for(auto poly_i = 0; poly_i < mesh->GetPolygonCount(); poly_i++)
			{
				AssetTriangle tri;
				if(material_element && material_element->GetMappingMode()==KFbxGeometryElement::eBY_POLYGON)
				{
					tri.material_index = material_element->GetIndexArray().GetAt(poly_i); 
				}
				tri.vertex_indices[0] = mesh->GetPolygonVertex(poly_i, 0);
				tri.vertex_indices[1] = mesh->GetPolygonVertex(poly_i, 1);
				tri.vertex_indices[2] = mesh->GetPolygonVertex(poly_i, 2);
				model->triangles.push_back(tri);
			}
			//skin
			auto skin_count = mesh->GetDeformerCount(KFbxDeformer::eSKIN);
			if(skin_count == 1)
			{
				auto skin = (KFbxSkin*) mesh->GetDeformer(0, KFbxDeformer::eSKIN);

				auto cluster_count = skin->GetClusterCount();
		
				int * vert_influences_count = new int[model->verticies.size()];
				memset(vert_influences_count, 0, sizeof(int) * model->verticies.size());

				for(auto cluster_i = 0; cluster_i < cluster_count; cluster_i++)
				{					
					auto cluster = skin->GetCluster(cluster_i);

					auto indices = cluster->GetControlPointIndices();
					auto weights = cluster->GetControlPointWeights();
					auto k = cluster->GetControlPointIndicesCount();
					assert(cluster->GetLinkMode() == KFbxCluster::ELinkMode::eNORMALIZE);
					assert(vert_count >= cluster->GetControlPointIndicesCount());
					for(int indice_i = 0; indice_i < cluster->GetControlPointIndicesCount(); indice_i++)
					{
						int vert_index = indices[indice_i];

						if(vert_influences_count[vert_index] > 3) continue;

						float weight = (float)weights[indice_i];

						int bone_index = vert_influences_count[vert_index];
						model->verticies[vert_index].joints[bone_index] = (unsigned int)cluster_i;
						model->verticies[vert_index].joint_weights[bone_index] = weight;
						vert_influences_count[vert_index]++;
						//at most 4 bones per vert
						assert(vert_influences_count[vert_index] < 5);
					}
				}
				delete [] vert_influences_count;
			}
			//materials
			auto mesh_node = mesh->GetNode();
			auto mat_count = mesh_node->GetMaterialCount();
			assert(mat_count > 0);
			for(auto mat_i = 0; mat_i < mat_count; mat_i++)
			{				
				auto material = mesh_node->GetMaterial(mat_i);
				auto diffuse_prop = material->FindProperty(KFbxSurfaceMaterial::sDiffuse);
				if(diffuse_prop.IsValid())
				{
					//only support 1 diffuse texture
					if(diffuse_prop.GetSrcObjectCount(KFbxFileTexture::ClassId) == 1)
					{						
						auto diffuse_file_texture = diffuse_prop.GetSrcObject(FBX_TYPE(KFbxFileTexture), 0);
						auto diffuse_file_name = string(diffuse_file_texture->GetFileName());
					
						wstring diffuse_file_name_ws;
						diffuse_file_name_ws.assign(diffuse_file_name.begin(), diffuse_file_name.end());
						model->materials.push_back(diffuse_file_name_ws);
					}
					else
					{
						model->materials.push_back(L"");
					}
				}
				else
				{
					model->materials.push_back(L"");
				}
			}
			return model;
		}
		//NEED
		shared_ptr<Model> asset_model_to_gfx_model(AssetModel & asset_model)
		{
			auto gfx_model = make_shared<Model>();

			gfx_model->vertices = (char*)malloc(asset_model.verticies.size() * sizeof(StandardAnimatedVertex));
			
			gfx_model->vertices_count = asset_model.verticies.size();
			gfx_model->vertices_stride = sizeof(StandardAnimatedVertex);
			auto verts = (StandardAnimatedVertex*)gfx_model->vertices;
			for(auto vert_i = 0; vert_i < asset_model.verticies.size(); vert_i++)
			{
				StandardAnimatedVertex & v_dst = verts[vert_i];
				AssetVertex & v_src = asset_model.verticies[vert_i];

				v_dst.position[0] = v_src.position[0];
				v_dst.position[1] = v_src.position[1];
				v_dst.position[2] = v_src.position[2];
				
				v_dst.normal[0] = v_src.normal[0];
				v_dst.normal[1] = v_src.normal[1];
				v_dst.normal[2] = v_src.normal[2];
				
				v_dst.uv[0] = v_src.uv[0];
				v_dst.uv[1] = v_src.uv[1];
				
				v_dst.bones[0] = v_src.joints[0];
				v_dst.bones[1] = v_src.joints[1];
				v_dst.bones[2] = v_src.joints[2];
				v_dst.bones[3] = v_src.joints[3];

				v_dst.bone_weights[0] = v_src.joint_weights[0];
				v_dst.bone_weights[1] = v_src.joint_weights[1];
				v_dst.bone_weights[2] = v_src.joint_weights[2];
				v_dst.bone_weights[3] = v_src.joint_weights[3];
			}
			//split model by material
			//into mesh parts
			vector<unsigned int> splitted_index_buffer;
			splitted_index_buffer.reserve(asset_model.triangles.size() * 3);
			for(auto material_i = 0; material_i < asset_model.materials.size(); material_i++)
			{
				gfx::MeshPart part;
				part.albedo = asset_model.materials[material_i];
				part.indices_offset = splitted_index_buffer.size();
				part.indices_count = 0;
				//for(auto tri : asset_model.triangles)
				for(auto it = asset_model.triangles.begin(); it != asset_model.triangles.end(); it++)
				{
					auto tri = *it;
					if(tri.material_index == material_i)
					{
						splitted_index_buffer.push_back(tri.vertex_indices[0]);
						splitted_index_buffer.push_back(tri.vertex_indices[1]);
						splitted_index_buffer.push_back(tri.vertex_indices[2]);
						part.indices_count += 3;
					}
				}
				gfx_model->parts.push_back(part);
			}
			gfx_model->indexes = make_shared<Data<unsigned int>>(splitted_index_buffer.size());
			std::copy(splitted_index_buffer.cbegin(), splitted_index_buffer.cend(), 
				gfx_model->indexes->ptr);

			return gfx_model;
		}
		KFbxNode * find_mesh_node(KFbxNode * node)
		{
			auto attribute = node->GetNodeAttribute();
			if(attribute != nullptr)
			{
				if(attribute->GetAttributeType() == KFbxNodeAttribute::EAttributeType::eMESH)
				{
					return node;
				}
			}
			for(int i = 0; i < node->GetChildCount(); i++) 
			{
				auto result = find_mesh_node(node->GetChild(i));
				if(result != nullptr) return result;
			}
			return nullptr;
		}

		shared_ptr<Model> load_animated_fbx_model(const char * path)
		{	

			auto sdk_manager = KFbxSdkManager::Create();
			vector<shared_ptr<Model>> splitted_models;
			
			auto scene = KFbxScene::Create(sdk_manager, "");
			auto importer = KFbxImporter::Create(sdk_manager, "");
			auto error = importer->Initialize(path);
			auto result = importer->Import(scene);
			importer->Destroy();
		
			KFbxGeometryConverter geo_converter(sdk_manager);;


			auto mesh_node = find_mesh_node(scene->GetRootNode());
			assert(mesh_node != nullptr);
			assert(geo_converter.TriangulateInPlace(mesh_node));
			auto mesh = mesh_node->GetMesh();
			assert(mesh->SplitPoints(KFbxLayerElement::ELayerElementType::eNORMAL));
			//need 1 uv / vert
			assert(mesh->SplitPoints(KFbxLayerElement::ELayerElementType::eDIFFUSE_TEXTURES));
			

			auto asset_model = mesh_to_asset_model(mesh);
			
			auto gfx_model = asset_model_to_gfx_model(*asset_model);
			gfx_model->animations = fill_animations(scene, mesh);
				
			
			sdk_manager->Destroy();
			return gfx_model;

		}
	}
}
