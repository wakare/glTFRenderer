#include "glTFMeshRawData.h"

#include "glTFScenePrimitive.h"

glTFMeshRawData::glTFMeshRawData(const glTFLoader& loader, const glTF_Primitive& primitive)
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
		primitive, vertex_buffer_size, vertexLayout, vertex_data_in_buffers);

	// NORMAL attribute
	_process_vertex_attribute(loader, glTF_Attribute_NORMAL::attribute_type_id, VertexAttributeType::VERTEX_NORMAL,
		primitive, vertex_buffer_size, vertexLayout, vertex_data_in_buffers);

	// TANGENT attribute
	_process_vertex_attribute(loader, glTF_Attribute_TANGENT::attribute_type_id, VertexAttributeType::VERTEX_TANGENT,
		primitive, vertex_buffer_size, vertexLayout, vertex_data_in_buffers);
                
	// TEXCOORD attribute
	_process_vertex_attribute(loader, glTF_Attribute_TEXCOORD_0::attribute_type_id, VertexAttributeType::VERTEX_TEXCOORD0,
		primitive, vertex_buffer_size, vertexLayout, vertex_data_in_buffers);

	vertex_buffer_data = std::make_shared<VertexBufferData>();
	vertex_buffer_data->data.reset(new char[vertex_buffer_size]);
	vertex_buffer_data->byte_size = vertex_buffer_size;
	vertex_buffer_data->vertex_count = vertex_buffer_size / vertexLayout.GetVertexStrideInBytes();
	vertex_buffer_data->layout = vertexLayout;

	position_only_data = std::make_shared<VertexBufferData>();
	const size_t position_only_data_size = vertex_buffer_data->vertex_count * 3 * sizeof(float);
	position_only_data->data.reset(new char[position_only_data_size]);
	position_only_data->byte_size = position_only_data_size;
	position_only_data->vertex_count = vertex_buffer_data->vertex_count;
	position_only_data->layout.elements.push_back({VertexAttributeType::VERTEX_POSITION, 12});
                
	char* vertex_data_start = vertex_buffer_data->data.get();
	char* position_only_data_start = position_only_data->data.get();
                
	for (size_t v = 0; v < vertex_buffer_data->vertex_count; ++v)
	{
		// Reformat vertex buffer data
		for (size_t i = 0; i < vertexLayout.elements.size(); ++i)
		{
			if (vertexLayout.elements[i].type == VertexAttributeType::VERTEX_POSITION)
			{
				// Position attribute component type should be float
				GLTF_CHECK(vertexLayout.elements[i].byte_size == 3 * sizeof(float));
				auto position = reinterpret_cast<float*>(vertex_data_in_buffers[i]);
				m_box.extend({position[0], position[1], position[2]});

				memcpy(position_only_data_start, vertex_data_in_buffers[i], vertexLayout.elements[i].byte_size);
				position_only_data_start += vertexLayout.elements[i].byte_size;
			}
                        
			memcpy(vertex_data_start, vertex_data_in_buffers[i], vertexLayout.elements[i].byte_size);
			vertex_data_in_buffers[i] += vertexLayout.elements[i].byte_size;
			vertex_data_start += vertexLayout.elements[i].byte_size;
		}
	}

	const auto& index_accessor = *loader.GetAccessors()[loader.ResolveIndex(primitive.indices)];
	const auto& index_buffer_view = *loader.GetBufferViews()[loader.ResolveIndex(index_accessor.buffer_view)];

	const size_t index_buffer_size = index_accessor.GetElementByteSize() * index_accessor.count;
                
	index_buffer_data = std::make_shared<IndexBufferData>();
	index_buffer_data->data.reset(new char[index_buffer_size]);
	glTFHandle tempIndexBufferViewHandle = index_buffer_view.buffer;
	tempIndexBufferViewHandle.node_index = loader.ResolveIndex(index_buffer_view.buffer);
	auto findIt = loader.GetBufferData().find(tempIndexBufferViewHandle);
	GLTF_CHECK(findIt != loader.GetBufferData().end());
	const char* bufferStart = findIt->second.get() + index_buffer_view.byte_offset + index_accessor.byte_offset;
	memcpy(index_buffer_data->data.get(), bufferStart, index_buffer_size);
	index_buffer_data->byte_size = index_buffer_size;
	index_buffer_data->index_count = index_accessor.count;
	index_buffer_data->format = index_accessor.component_type ==
		glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EUnsignedShort ?
		RHIDataFormat::R16_UINT : RHIDataFormat::R32_UINT;
}
