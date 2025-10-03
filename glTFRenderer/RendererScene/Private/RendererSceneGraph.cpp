#include "RendererSceneGraph.h"
#include <glm/glm/gtx/quaternion.hpp>
#include <utility>
#include <glm/glm/gtx/matrix_decompose.hpp>

#include "RendererSceneCommon.h"

RendererSceneMesh::RendererSceneMesh(const glTFLoader& loader, const glTF_Primitive& primitive)
{
    size_t vertex_buffer_size = 0;
	std::vector<char*> vertex_data_in_buffers;

	static auto _process_vertex_attribute = [](const glTFLoader& source_loader, glTFAttributeId attribute_ID, VertexAttributeType attribute_type,
		const glTF_Primitive& source_primitive, size_t& out_vertex_buffer_size, VertexLayoutDeclaration& out_vertex_layout,
		std::vector<char*>& out_vertex_data_infos)
	{
		const auto itPosition = source_primitive.attributes.find(attribute_ID);
		if (itPosition != source_primitive.attributes.end())
		{
			const glTFHandle accessorHandle = itPosition->second; 
			const auto& vertexAccessor = *source_loader.GetAccessors()[source_loader.ResolveIndex(accessorHandle)];
			out_vertex_layout.elements.push_back({attribute_type, vertexAccessor.GetElementByteSize()});
			out_vertex_buffer_size += vertexAccessor.count * vertexAccessor.GetElementByteSize();

			const auto& vertexBufferView = *source_loader.GetBufferViews()[source_loader.ResolveIndex(vertexAccessor.buffer_view)];
			glTFHandle tempVertexBufferViewHandle = vertexBufferView.buffer;
			tempVertexBufferViewHandle.node_index = source_loader.ResolveIndex(vertexBufferView.buffer);
			const auto findIt = source_loader.GetBufferData().find(tempVertexBufferViewHandle);
			GLTF_CHECK(findIt != source_loader.GetBufferData().end());
			char* position_data = findIt->second.get() + vertexBufferView.byte_offset + vertexAccessor.byte_offset;
			out_vertex_data_infos.push_back(position_data);
		}
	};
                
	// POSITION attribute
	_process_vertex_attribute(loader, glTF_Attribute_POSITION::attribute_type_id, VertexAttributeType::VERTEX_POSITION,
		primitive, vertex_buffer_size, m_vertex_layout, vertex_data_in_buffers);

	// NORMAL attribute
	_process_vertex_attribute(loader, glTF_Attribute_NORMAL::attribute_type_id, VertexAttributeType::VERTEX_NORMAL,
		primitive, vertex_buffer_size, m_vertex_layout, vertex_data_in_buffers);

	// TANGENT attribute
	_process_vertex_attribute(loader, glTF_Attribute_TANGENT::attribute_type_id, VertexAttributeType::VERTEX_TANGENT,
		primitive, vertex_buffer_size, m_vertex_layout, vertex_data_in_buffers);
                
	// TEXCOORD attribute
	_process_vertex_attribute(loader, glTF_Attribute_TEXCOORD_0::attribute_type_id, VertexAttributeType::VERTEX_TEXCOORD0,
		primitive, vertex_buffer_size, m_vertex_layout, vertex_data_in_buffers);

	m_vertex_buffer_data = std::make_shared<VertexBufferData>();
	m_vertex_buffer_data->data.reset(new char[vertex_buffer_size]);
	m_vertex_buffer_data->byte_size = vertex_buffer_size;
	m_vertex_buffer_data->vertex_count = vertex_buffer_size / m_vertex_layout.GetVertexStrideInBytes();
	m_vertex_buffer_data->layout = m_vertex_layout;

	m_position_only_data = std::make_shared<VertexBufferData>();
	const size_t position_only_data_size = m_vertex_buffer_data->vertex_count * 3 * sizeof(float);
	m_position_only_data->data.reset(new char[position_only_data_size]);
	m_position_only_data->byte_size = position_only_data_size;
	m_position_only_data->vertex_count = m_vertex_buffer_data->vertex_count;
	m_position_only_data->layout.elements.push_back({VertexAttributeType::VERTEX_POSITION, 12});
                
	char* vertex_data_start = m_vertex_buffer_data->data.get();
	char* position_only_data_start = m_position_only_data->data.get();
                
	for (size_t v = 0; v < m_vertex_buffer_data->vertex_count; ++v)
	{
		// Reformat vertex buffer data
		for (size_t i = 0; i < m_vertex_layout.elements.size(); ++i)
		{
			if (m_vertex_layout.elements[i].type == VertexAttributeType::VERTEX_POSITION)
			{
				// Position attribute component type should be float
				GLTF_CHECK(m_vertex_layout.elements[i].byte_size == 3 * sizeof(float));
				auto position = reinterpret_cast<float*>(vertex_data_in_buffers[i]);
				m_box.extend({position[0], position[1], position[2]});

				memcpy(position_only_data_start, vertex_data_in_buffers[i], m_vertex_layout.elements[i].byte_size);
				position_only_data_start += m_vertex_layout.elements[i].byte_size;
			}
                        
			memcpy(vertex_data_start, vertex_data_in_buffers[i], m_vertex_layout.elements[i].byte_size);
			vertex_data_in_buffers[i] += m_vertex_layout.elements[i].byte_size;
			vertex_data_start += m_vertex_layout.elements[i].byte_size;
		}
	}

	const auto& index_accessor = *loader.GetAccessors()[loader.ResolveIndex(primitive.indices)];
	const auto& index_buffer_view = *loader.GetBufferViews()[loader.ResolveIndex(index_accessor.buffer_view)];

	const size_t index_buffer_size = index_accessor.GetElementByteSize() * index_accessor.count;
                
	m_index_buffer_data = std::make_shared<IndexBufferData>();
	m_index_buffer_data->data.reset(new char[index_buffer_size]);
	glTFHandle tempIndexBufferViewHandle = index_buffer_view.buffer;
	tempIndexBufferViewHandle.node_index = loader.ResolveIndex(index_buffer_view.buffer);
	auto findIt = loader.GetBufferData().find(tempIndexBufferViewHandle);
	GLTF_CHECK(findIt != loader.GetBufferData().end());
	const char* bufferStart = findIt->second.get() + index_buffer_view.byte_offset + index_accessor.byte_offset;
	memcpy(m_index_buffer_data->data.get(), bufferStart, index_buffer_size);
	m_index_buffer_data->byte_size = index_buffer_size;
	m_index_buffer_data->index_count = index_accessor.count;
	m_index_buffer_data->format = index_accessor.component_type ==
		glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EUnsignedShort ?
		RHIDataFormat::R16_UINT : RHIDataFormat::R32_UINT;
}

RendererSceneMesh::RendererSceneMesh(VertexLayoutDeclaration vertex_layout, std::shared_ptr<VertexBufferData> vertex_buffer,
                                 std::shared_ptr<IndexBufferData> index_buffer)
    : m_vertex_layout(std::move(vertex_layout))
    , m_vertex_buffer_data(std::move(vertex_buffer))
    , m_index_buffer_data(std::move(index_buffer))
{
    
}

void RendererSceneMesh::SetMaterial(std::shared_ptr<MaterialBase> material)
{
	// Now cannot set material twice
	GLTF_CHECK(!m_material);
	m_material = std::move(material);
}

bool RendererSceneMesh::HasMaterial() const
{
	return m_material != nullptr;
}

const MaterialBase& RendererSceneMesh::GetMaterial() const
{
	GLTF_CHECK(HasMaterial());
	return *m_material;
}

std::shared_ptr<RendererSceneNodeTransform> RendererSceneNodeTransform::identity_transform = std::make_shared<RendererSceneNodeTransform>();

RendererSceneNodeTransform::RendererSceneNodeTransform(const glm::fmat4& transform)
    : m_cached_transform(transform)
{
	glm::fvec3 skew;
	glm::fvec4 perspective;
    bool decomposed = glm::decompose(m_cached_transform, m_scale, m_quat, m_translation, skew, perspective);
	GLTF_CHECK(decomposed);

	m_euler_angles = eulerAngles(m_quat);
}

void RendererSceneNodeTransform::Translate(const glm::fvec3& translation)
{
    m_translation = translation;
    MarkTransformDirty();
}

void RendererSceneNodeTransform::TranslateOffset(const glm::fvec3& translation)
{
    m_translation += translation;
    MarkTransformDirty();
}

void RendererSceneNodeTransform::RotateEulerAngle(const glm::fvec3& euler_angle)
{
    m_euler_angles = euler_angle;
    m_quat = m_euler_angles;
    MarkTransformDirty();
}

void RendererSceneNodeTransform::RotateEulerAngleOffset(const glm::fvec3& euler_angle)
{
    m_euler_angles += euler_angle;
    m_quat = m_euler_angles;
    MarkTransformDirty();

}

void RendererSceneNodeTransform::Scale(const glm::fvec3& scale)
{
    m_scale = scale;
    MarkTransformDirty();
}

void RendererSceneNodeTransform::MarkTransformDirty()
{
    m_transform_dirty = true;
}

bool RendererSceneNodeTransform::IsTransformDirty() const
{
	return m_transform_dirty;
}

glm::fmat4 RendererSceneNodeTransform::GetTransformMatrix()
{
    if (m_transform_dirty)
    {
        const glm::mat4 rotation_matrix = glm::toMat4(m_quat);
        const glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0f), m_scale);
        const glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0f), m_translation);
        
        m_cached_transform = translation_matrix * rotation_matrix * scale_matrix;
        
        m_transform_dirty = false;
    }

    return m_cached_transform;
}

glm::fvec3 RendererSceneNodeTransform::GetTranslation() const
{
	return m_translation;	
}

glm::fvec3 RendererSceneNodeTransform::GetRotationEulerAngle() const
{
	return m_euler_angles;
}

glm::fvec3 RendererSceneNodeTransform::GetScale() const
{
	return m_scale;
}

RendererSceneNode::RendererSceneNode(std::weak_ptr<RendererSceneNode> parent, std::shared_ptr<RendererSceneNodeTransform> local_transform)
    : m_parent(std::move(parent))
	, m_local_transform(std::move(local_transform))
{
}

void RendererSceneNode::MarkDirty(bool recursive)
{
    m_dirty = true;
	if (recursive)
	{
		for (auto& child : m_children)
		{
			child.second->MarkDirty(recursive);
		}
	}
}

bool RendererSceneNode::IsDirty() const
{
    return m_dirty;
}

void RendererSceneNode::ResetDirty(bool recursive)
{
    m_dirty = false;
	if (recursive)
	{
		for (auto& child : m_children)
		{
			child.second->ResetDirty(recursive);
		}
	}
}

void RendererSceneNode::AddMesh(std::shared_ptr<RendererSceneMesh> mesh)
{
    m_meshes.push_back(mesh);
}

void RendererSceneNode::SetLocalTransform(std::shared_ptr<RendererSceneNodeTransform> transform)
{
    m_local_transform = std::move(transform);
}

bool RendererSceneNode::HasMesh() const
{
    return !m_meshes.empty();
}

const std::vector<std::shared_ptr<RendererSceneMesh>>& RendererSceneNode::GetMeshes() const
{
    return m_meshes;
}

const RendererSceneNodeTransform& RendererSceneNode::GetLocalTransform() const
{
    return *m_local_transform;
}

glm::fmat4x4 RendererSceneNode::GetAbsoluteTransform()
{
	glm::fmat4x4 result = m_local_transform->GetTransformMatrix();
	if (!m_parent.expired())
	{
		auto parent_node = m_parent.lock();
		result = result * parent_node->GetAbsoluteTransform();
	}
	
	return result;
}

void RendererSceneNode::AddChild(std::shared_ptr<RendererSceneNode> child)
{
    if (!m_children.contains(child->GetID()))
    {
        m_children.emplace(child->GetID(), child);
    }
}

void RendererSceneNode::Traverse(const std::function<bool(RendererSceneNode& node)>& traverse_function)
{
	GLTF_CHECK(traverse_function);

	bool stop = traverse_function(*this);
	if (!stop)
	{
		for (auto& child : m_children)
		{
			child.second->Traverse(traverse_function);	
		}
	}
}

void RendererSceneNode::ConstTraverse(const std::function<bool(const RendererSceneNode& node)>& traverse_function) const
{
	GLTF_CHECK(traverse_function);

	bool stop = traverse_function(*this);
	if (!stop)
	{
		for (auto& child : m_children)
		{
			child.second->ConstTraverse(traverse_function);	
		}
	}
}

RendererSceneGraph::RendererSceneGraph()
{
    m_root_node = std::make_shared<RendererSceneNode>(std::weak_ptr<RendererSceneNode>(), RendererSceneNodeTransform::identity_transform);
}

bool RendererSceneGraph::InitializeRootNodeWithSceneFile_glTF(const glTFLoader& loader)
{
    const auto& scene_node = loader.GetDefaultScene();
    for (const auto& root_node : scene_node.root_nodes)
    {
        std::shared_ptr<RendererSceneNode> root_scene_root_node = std::make_shared<RendererSceneNode>(m_root_node);
        RecursiveInitSceneNodeFromGLTFLoader(loader, root_node, *root_scene_root_node);
    	m_root_node->AddChild(root_scene_root_node);
    }
    
    return true;
}

RendererSceneNode& RendererSceneGraph::GetRootNode()
{
	return *m_root_node;
}

const RendererSceneNode& RendererSceneGraph::GetRootNode() const
{
	return *m_root_node;
}

const std::map<RendererUniqueObjectID, std::shared_ptr<RendererSceneMesh>>& RendererSceneGraph::GetMeshes() const
{
	return m_meshes;
}

const std::map<RendererUniqueObjectID, std::shared_ptr<MaterialBase>>& RendererSceneGraph::GetMaterials() const
{
	return m_mesh_materials;
}

void RendererSceneGraph::RecursiveInitSceneNodeFromGLTFLoader(const glTFLoader& loader, const glTFHandle& handle,
                                                              RendererSceneNode& scene_node)
{
	const auto& node = loader.GetNodes()[loader.ResolveIndex(handle)];
	scene_node.SetLocalTransform(std::make_shared<RendererSceneNodeTransform>(node->transform.GetMatrix()));

	std::map<glTFHandle, std::shared_ptr<MaterialBase>> mesh_materials;
	for (const auto& mesh_handle : node->meshes)
	{
		if (mesh_handle.IsValid())
		{
			const auto& mesh = *loader.GetMeshes()[loader.ResolveIndex(mesh_handle)];
            
			for (const auto& primitive : mesh.primitives)
			{
				if (!m_meshes.contains(primitive.Hash()))
				{
					std::shared_ptr<RendererSceneMesh> render_scene_mesh = std::make_shared<RendererSceneMesh>(loader, primitive);

					if (!mesh_materials.contains(primitive.material))
					{
						std::shared_ptr<MaterialBase> mesh_material = std::make_shared<MaterialBase>();

						auto material_id = primitive.material;
						const auto& source_material =
						*loader.GetMaterials()[loader.ResolveIndex(material_id)];

						// Base color texture setting
						const auto base_color_texture_handle = source_material.pbr.base_color_texture.index;
						if (base_color_texture_handle.IsValid())
						{
							const auto& base_color_texture = *loader.GetTextures()[loader.ResolveIndex(base_color_texture_handle)];
							const auto& texture_image = *loader.GetImages()[loader.ResolveIndex(base_color_texture.source)];
							GLTF_CHECK(!texture_image.uri.empty());

							std::string texture_uri = loader.GetSceneFileDirectory() + texture_image.uri;
							mesh_material->SetParameter(MaterialBase::MaterialParameterUsage::BASE_COLOR, std::make_shared<MaterialParameter>(texture_uri));    
						}
						else
						{
							mesh_material->SetParameter(MaterialBase::MaterialParameterUsage::BASE_COLOR, std::make_shared<MaterialParameter>(source_material.pbr.base_color_factor));
						}

						// Normal texture setting
						const auto normal_texture_handle = source_material.normal_texture.index; 
						if (normal_texture_handle.IsValid())
						{
							const auto& normal_texture = *loader.GetTextures()[loader.ResolveIndex(normal_texture_handle)];
							const auto& texture_image = *loader.GetImages()[loader.ResolveIndex(normal_texture.source)];
							GLTF_CHECK(!texture_image.uri.empty());
                    	
							std::string texture_uri = loader.GetSceneFileDirectory() + texture_image.uri;
							mesh_material->SetParameter(MaterialBase::MaterialParameterUsage::NORMAL, std::make_shared<MaterialParameter>(texture_uri));
						}

						// Metallic roughness texture setting
						const auto metallic_roughness_texture_handle = source_material.pbr.metallic_roughness_texture.index; 
						if (metallic_roughness_texture_handle.IsValid())
						{
							const auto& metallic_roughness_texture = *loader.GetTextures()[loader.ResolveIndex(metallic_roughness_texture_handle)];
							const auto& texture_image = *loader.GetImages()[loader.ResolveIndex(metallic_roughness_texture.source)];
							GLTF_CHECK(!texture_image.uri.empty());
                    	
							std::string texture_uri = loader.GetSceneFileDirectory() + texture_image.uri;
							mesh_material->SetParameter(MaterialBase::MaterialParameterUsage::METALLIC_ROUGHNESS, std::make_shared<MaterialParameter>(texture_uri));
						}

						render_scene_mesh->SetMaterial(mesh_materials.at(primitive.material));
						m_meshes.emplace(primitive.Hash(), render_scene_mesh);
					}

					scene_node.AddMesh(m_meshes.at(primitive.Hash()));
				}
			}
		}

		for (auto& material : mesh_materials)
		{
			m_mesh_materials.insert({material.second->GetID(), material.second});
		}
	}
}
