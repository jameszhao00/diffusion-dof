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

using namespace std;
using namespace gfx;
namespace asset
{		
	namespace fbx
	{
		
		//NEED
		//doesn't need to worry about mesh splitting
		list<shared_ptr<Animation>> fill_animations(KFbxScene * scene, KFbxMesh * mesh)
		{		
			list<shared_ptr<Animation>> animations;
			
			FbxArray<FbxString*> anim_stack_names;
			scene->FillAnimStackNameArray(anim_stack_names);
		
			auto skin_count = mesh->GetDeformerCount(FbxDeformer::eSkin);
			if(skin_count == 1)
			{
				auto skin = (KFbxSkin*) mesh->GetDeformer(0, FbxDeformer::eSkin);

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

							assert(cluster->GetLinkMode() == KFbxCluster::ELinkMode::eNormalize);
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
		struct AssetVertex
		{
			//float position[3];
			vec3 position;
			//float normal[3];
			vec3 normal;
			//float uv[2];
			vec2 uv;
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
			//unused... please remove
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
		shared_ptr<AssetModel> mesh_to_asset_model(KFbxMesh * mesh, KFbxXMatrix node_to_world)
		{
			assert(mesh->IsTriangleMesh());
	
			shared_ptr<AssetModel> model(new AssetModel());

			auto vert_count = mesh->GetControlPointsCount();	

			auto normals_element = mesh->GetLayer(0)->GetNormals();
			auto uvs_element = mesh->GetLayer(0)->GetUVs();

			if(normals_element) assert(normals_element->GetMappingMode() == KFbxLayerElement::eByControlPoint);
			if(uvs_element) assert(uvs_element->GetMappingMode() == KFbxLayerElement::eByControlPoint);

			for(auto vert_i = 0; vert_i < vert_count; vert_i++)
			{
				auto r = mesh->GetControlPointAt(vert_i);
				auto vert_pos = node_to_world.MultT(r);			
				AssetVertex vert;	
				//negate z b/c we converted to opengl coord sys
				//the directx conversion doesn't work...	
				vert.position = vec3(vert_pos.mData[0], vert_pos.mData[1], -vert_pos.mData[2]);

				if(normals_element)
				{
					KFbxVector4 normal;
					if(normals_element->GetReferenceMode() == KFbxLayerElement::eDirect)
					{
						normal = normals_element->GetDirectArray().GetAt(vert_i);
					}
					else
					{					
						auto index = normals_element->GetIndexArray().GetAt(vert_i);
						normal = normals_element->GetDirectArray().GetAt(index);
					}
					normal = node_to_world.MultR(normal);
					//negate z b/c we converted to opengl coord sys
					//the directx conversion doesn't work...	
					vert.normal = vec3(normal.mData[0], normal.mData[1], -normal.mData[2]);
				}
				if(uvs_element)
				{
					KFbxVector2 uv;
					if(uvs_element->GetReferenceMode() == KFbxLayerElement::eDirect)
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
				if(material_element && material_element->GetMappingMode()==FbxGeometryElement::eByPolygon)
				{
					tri.material_index = material_element->GetIndexArray().GetAt(poly_i); 
				}
				//change winding order b/c we're in OPENGL mode...
				tri.vertex_indices[0] = mesh->GetPolygonVertex(poly_i, 2);
				tri.vertex_indices[1] = mesh->GetPolygonVertex(poly_i, 1);
				tri.vertex_indices[2] = mesh->GetPolygonVertex(poly_i, 0);
				model->triangles.push_back(tri);
			}
			//skin
			auto skin_count = mesh->GetDeformerCount(KFbxDeformer::eSkin);
			if(skin_count == 1)
			{
				auto skin = (KFbxSkin*) mesh->GetDeformer(0, KFbxDeformer::eSkin);

				auto cluster_count = skin->GetClusterCount();
		
				int * vert_influences_count = new int[model->verticies.size()];
				memset(vert_influences_count, 0, sizeof(int) * model->verticies.size());

				for(auto cluster_i = 0; cluster_i < cluster_count; cluster_i++)
				{					
					auto cluster = skin->GetCluster(cluster_i);

					auto indices = cluster->GetControlPointIndices();
					auto weights = cluster->GetControlPointWeights();
					auto k = cluster->GetControlPointIndicesCount();
					assert(cluster->GetLinkMode() == KFbxCluster::ELinkMode::eNormalize);
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
		void find_nodes(KFbxNode* node, FbxNodeAttribute::EType type, list<KFbxNode*>* output)
		{
			auto attribute = node->GetNodeAttribute();
			if(attribute != nullptr)
			{
				if(attribute->GetAttributeType() == type)
				{
					output->push_back(node);
				}
			}
			for(int i = 0; i < node->GetChildCount(); i++) 
			{
				find_nodes(node->GetChild(i), type, output);
			}
		}
		KFbxNode * find_mesh_node(KFbxNode * node)
		{
			auto attribute = node->GetNodeAttribute();
			if(attribute != nullptr)
			{
				if(attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
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


		vec3 to_glm(fbxDouble3 d3)
		{
			return vec3(d3.mData[0], d3.mData[1], d3.mData[2]);
		}
		shared_ptr<Model> load_animated_fbx_model(const char * path, vector<Camera>* cameras)
		{	

			auto sdk_manager = KFbxSdkManager::Create();
			vector<shared_ptr<Model>> splitted_models;
			
			auto scene = KFbxScene::Create(sdk_manager, "");
			auto importer = KFbxImporter::Create(sdk_manager, "");
			auto error = importer->Initialize(path);
			auto result = importer->Import(scene);
			importer->Destroy();

			auto current_axis = scene->GetGlobalSettings().GetAxisSystem();			
			auto current_unit = scene->GetGlobalSettings().GetSystemUnit();

			auto desired_unit(KFbxSystemUnit::cm);
			KFbxAxisSystem desired_axis(KFbxAxisSystem::OpenGL);

			if (current_unit != desired_unit)
			{
				desired_unit.ConvertScene(scene);
			}
			if (current_axis != desired_axis)
			{
				desired_axis.ConvertScene(scene);
			}			
		
			FbxGeometryConverter geo_converter(sdk_manager);;	
			

			auto mesh_node = find_mesh_node(scene->GetRootNode());
			assert(mesh_node != nullptr);
			assert(geo_converter.TriangulateInPlace(mesh_node));
			auto mesh = mesh_node->GetMesh();
			assert(mesh->SplitPoints(FbxLayerElement::eNormal));
			//need 1 uv / vert
			assert(mesh->SplitPoints(FbxLayerElement::eTextureDiffuse));

			//all the PREPROCESSING's done	
			list<KFbxNode*> camera_nodes;
			find_nodes(scene->GetRootNode(), KFbxNodeAttribute::eCamera, &camera_nodes);
			//this camera code doesn't work ;(
			for(auto it = camera_nodes.cbegin(); it != camera_nodes.cend(); it++)
			{
				Camera cam;
				auto camera_node = (*it);
				auto camera = camera_node->GetCamera();
				cam.eye = to_glm(camera->Position.Get());
				cam.up = to_glm(camera->UpVector.Get());
				KFbxXMatrix rot;
				auto target = camera_node->GetTarget();
				assert(target != nullptr);
				cam.focus = to_glm(scene->GetEvaluator()->GetNodeGlobalTransform(target).GetT());
				cam.n = camera->GetNearPlane(); cam.f = camera->GetFarPlane();

				auto mode = camera->GetAspectRatioMode();
				assert(mode == FbxCamera::eWindowSize);
				
				cam.aspect_ratio = camera->AspectWidth.Get() / camera->AspectHeight.Get();
				

				//from FBX SDK
				//dealing with aperture ratios... and FOVs
				//(a giant mess)

				{
					//align up vector

					auto lForward = cam.focus - cam.eye;
					lForward = glm::normalize(lForward);
					auto lRight = cross(lForward, cam.up);
					lRight = normalize(lRight);
					//flipped the cros sproduct order
					cam.up = cross(lForward, lRight);
					cam.up = normalize(cam.up);



					auto lCamera = camera;
					double lFilmHeight = lCamera->GetApertureHeight();
					double lFilmWidth = lCamera->GetApertureWidth() * lCamera->GetSqueezeRatio();
					//here we use Height : Width
					double lApertureRatio = lFilmHeight / lFilmWidth;

					//change the aspect ratio to Height : Width
					double lAspectRatio = 1 / cam.aspect_ratio;
					//revise the aspect ratio and aperture ratio
					FbxCamera::EGateFit lCameraGateFit = lCamera->GateFit.Get();
					switch( lCameraGateFit)
					{

					case FbxCamera::eFitFill:
						if( lApertureRatio > lAspectRatio)  // the same as eHORIZONTAL_FIT
						{
							lFilmHeight = lFilmWidth * lAspectRatio;
							lCamera->SetApertureHeight( lFilmHeight);
							lApertureRatio = lFilmHeight / lFilmWidth;
						}
						else if( lApertureRatio < lAspectRatio) //the same as eVERTICAL_FIT
						{
							lFilmWidth = lFilmHeight / lAspectRatio;
							lCamera->SetApertureWidth( lFilmWidth);
							lApertureRatio = lFilmHeight / lFilmWidth;
						}
						break;
					case FbxCamera::eFitVertical:
						lFilmWidth = lFilmHeight / lAspectRatio;
						lCamera->SetApertureWidth( lFilmWidth);
						lApertureRatio = lFilmHeight / lFilmWidth;
						break;
					case KFbxCamera::eFitHorizontal:
						lFilmHeight = lFilmWidth * lAspectRatio;
						lCamera->SetApertureHeight( lFilmHeight);
						lApertureRatio = lFilmHeight / lFilmWidth;
						break;
					case KFbxCamera::eFitStretch:
						lAspectRatio = lApertureRatio;
						break;
					case KFbxCamera::eFitOverscan:
						if( lFilmWidth > lFilmHeight)
						{
							lFilmHeight = lFilmWidth * lAspectRatio;
						}
						else
						{
							lFilmWidth = lFilmHeight / lAspectRatio;
						}
						lApertureRatio = lFilmHeight / lFilmWidth;
						break;
					case KFbxCamera::eFitNone:
					default:
						break;
					}
					//change the aspect ratio to Width : Height
					lAspectRatio = 1 / lAspectRatio;

					//dealing with fov
					double lFieldOfViewX = 0.0;
					double lFieldOfViewY=0.0;

					#define K_PI_180		        .017453292519943295769236907684886127134428718885417
					#define K_180_PI		        57.295779513082320876798154814105170332405472466565
					#define HFOV2VFOV(h, ar) (2.0 * atan((ar) * tan( (h * K_PI_180) * 0.5)) * K_180_PI) //ar : aspectY / aspectX
					#define VFOV2HFOV(v, ar) (2.0 * atan((ar) * tan( (v * K_PI_180) * 0.5)) * K_180_PI) //ar : aspectX / aspectY
					if ( lCamera->GetApertureMode() == FbxCamera::eVertical)
					{
						lFieldOfViewY = lCamera->FieldOfView.Get();
						lFieldOfViewX = VFOV2HFOV( lFieldOfViewY, 1 / lApertureRatio);
					}
					else if (lCamera->GetApertureMode() == FbxCamera::eHorizontal)
					{
						lFieldOfViewX = lCamera->FieldOfView.Get(); //get HFOV
						lFieldOfViewY = HFOV2VFOV( lFieldOfViewX, lApertureRatio);
					}
					else if (lCamera->GetApertureMode() == FbxCamera::eFocalLength)
					{
						lFieldOfViewX = lCamera->ComputeFieldOfView(lCamera->FocalLength.Get());    //get HFOV
						lFieldOfViewY = HFOV2VFOV( lFieldOfViewX, lApertureRatio);
					}
					else if (lCamera->GetApertureMode() == FbxCamera::eHorizAndVert) {
						lFieldOfViewX = lCamera->FieldOfViewX.Get();
						lFieldOfViewY = lCamera->FieldOfViewY.Get();
					}
					cam.fov = lFieldOfViewY;
				}
				cameras->push_back(cam);
			}

			auto & node_world_transform = scene->GetEvaluator()->GetNodeGlobalTransform(mesh_node);
			auto asset_model = mesh_to_asset_model(mesh, node_world_transform);
			
			auto gfx_model = asset_model_to_gfx_model(*asset_model);
			gfx_model->animations = fill_animations(scene, mesh);
				
			
			sdk_manager->Destroy();
			return gfx_model;

		}
	}
}
