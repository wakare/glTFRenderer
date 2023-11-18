#include "glTFGraphicsPassMeshBase.h"

#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMesh.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMeshInfo.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

glTFGraphicsPassMeshBase::glTFGraphicsPassMeshBase()
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMesh>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMeshInfo>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<MeshInstanceInputData>>());
}

bool glTFGraphicsPassMeshBase::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::InitPass(resource_manager))
    
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSceneMeshInfo>()->UpdateSceneMeshData(resource_manager.GetMeshManager()))
    
    return true;
}

bool glTFGraphicsPassMeshBase::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RHIUtils::Instance().SetPrimitiveTopology( command_list, RHIPrimitiveTopologyType::TRIANGLELIST);

    if (!m_command_signature)
    {
        IRHICommandSignatureDesc command_signature_desc;
        RHIIndirectArgumentDesc vertex_buffer_desc;
    
        vertex_buffer_desc.type = RHIIndirectArgType::VertexBufferView;
        vertex_buffer_desc.desc.vertex_buffer.slot = 0;

        RHIIndirectArgumentDesc vertex_buffer_for_instancing_desc;
    
        vertex_buffer_for_instancing_desc.type = RHIIndirectArgType::VertexBufferView;
        vertex_buffer_for_instancing_desc.desc.vertex_buffer.slot = 1;

        RHIIndirectArgumentDesc index_buffer_desc;
        index_buffer_desc.type = RHIIndirectArgType::IndexBufferView;

        RHIIndirectArgumentDesc per_draw_constant_buffer_view_desc;
        per_draw_constant_buffer_view_desc.type = RHIIndirectArgType::ConstantBufferView;
        per_draw_constant_buffer_view_desc.desc.constant_buffer_view.root_parameter_index = GetRenderInterface<glTFRenderInterfaceSceneMesh>()->GetRSAllocation().parameter_index;
            
        RHIIndirectArgumentDesc draw_desc;
        draw_desc.type = RHIIndirectArgType::DrawIndexed;
    
        command_signature_desc.m_indirect_arguments.push_back(vertex_buffer_desc);
        command_signature_desc.m_indirect_arguments.push_back(vertex_buffer_for_instancing_desc);
        command_signature_desc.m_indirect_arguments.push_back(index_buffer_desc);
        command_signature_desc.m_indirect_arguments.push_back(per_draw_constant_buffer_view_desc);
        command_signature_desc.m_indirect_arguments.push_back(draw_desc);
        command_signature_desc.stride = sizeof(MeshIndirectDrawCommand);

        m_command_signature = RHIResourceFactory::CreateRHIResource<IRHICommandSignature>();
        m_command_signature->SetCommandSignatureDesc(command_signature_desc);
        RETURN_IF_FALSE(m_command_signature->InitCommandSignature(resource_manager.GetDevice(), m_root_signature_helper.GetRootSignature()))

        // TODO: resize buffer
        m_indirect_argument_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        const RHIBufferDesc indirect_arguments_buffer_desc = {L"indirect_arguments_buffer", 1024ull * 64,
       1, 1, RHIBufferType::Upload, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
        
        RETURN_IF_FALSE(m_indirect_argument_buffer->InitGPUBuffer(resource_manager.GetDevice(), indirect_arguments_buffer_desc ))
    }

    return true;
}

bool glTFGraphicsPassMeshBase::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::RenderPass(resource_manager))    

    auto& command_list = resource_manager.GetCommandListForRecord();

    RETURN_IF_FALSE(InitVertexAndInstanceBufferForFrame(resource_manager))
    const auto& mesh_render_resources = resource_manager.GetMeshManager().GetMeshRenderResources();

    if (UsingIndirectDraw())
    {
        m_indirect_arguments.clear();
        
        // Gather all mesh for indirect drawing
        for (const auto& instance : m_instance_draw_infos)
        {
            const auto& mesh_data = mesh_render_resources.find(instance.first);
            
            GetRenderInterface<glTFRenderInterfaceSceneMesh>()->UploadCPUBuffer(&instance.second.second, instance.first, sizeof(unsigned));
            
            MeshIndirectDrawCommand command
            (
                *mesh_data->second.mesh_vertex_buffer_view,
                *m_instance_buffer_view,
                *mesh_data->second.mesh_index_buffer_view,
                {GetRenderInterface<glTFRenderInterfaceSceneMesh>()->GetGPUHandle(instance.first)}
            );
            command.draw_command_argument.IndexCountPerInstance = mesh_data->second.mesh_index_count;
            command.draw_command_argument.InstanceCount = instance.second.first;
            command.draw_command_argument.StartInstanceLocation = instance.second.second;
            command.draw_command_argument.BaseVertexLocation = 0;
            command.draw_command_argument.StartIndexLocation = 0;

            m_indirect_arguments.push_back(command);
        }

        // Copy indirect command to gpu buffer
        // TODO: Resize buffer
        RETURN_IF_FALSE(m_indirect_argument_buffer->UploadBufferFromCPU(m_indirect_arguments.data(), 0, sizeof(MeshIndirectDrawCommand) * m_indirect_arguments.size()))
        
        if (UsingIndirectDrawCulling())
        {
            
        }
        else
        {
            RHIUtils::Instance().ExecuteIndirect(command_list, *m_command_signature, m_indirect_arguments.size(), *m_indirect_argument_buffer, 0);    
        }
    }
    else
    {
        for (const auto& instance : m_instance_draw_infos)
        {
            const auto& mesh_data = mesh_render_resources.find(instance.first);
            if (mesh_data == mesh_render_resources.end())
            {
                // No valid instance data exists..
                continue;
            }
        
            if (!mesh_data->second.mesh->IsVisible())
            {
                continue;
            }
        
            RETURN_IF_FALSE(BeginDrawMesh(resource_manager, mesh_data->second, instance.second.second))
        
            //RHIUtils::Instance().SetVertexBufferView(command_list, 0, *mesh_data->second.mesh_vertex_buffer_view);
            //RHIUtils::Instance().SetVertexBufferView(command_list, 1, *m_instance_buffer_view);
            RHIUtils::Instance().SetIndexBufferView(command_list, *mesh_data->second.mesh_index_buffer_view);
        
            RHIUtils::Instance().DrawIndexInstanced(command_list,
                mesh_data->second.mesh_index_count, instance.second.first,
                0, 0,
                instance.second.second);

            //RETURN_IF_FALSE(EndDrawMesh(resource_manager, mesh_data->second, instance.second))
        }    
    }

    return true;
}

bool glTFGraphicsPassMeshBase::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::SetupRootSignature(resource_manager))

    return true;
}

bool glTFGraphicsPassMeshBase::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassBase::SetupPipelineStateObject(resource_manager))

    return true;
}

bool glTFGraphicsPassMeshBase::BeginDrawMesh(glTFRenderResourceManager& resource_manager, const glTFMeshRenderResource& mesh_data, unsigned instance_offset)
{
    // Upload constant buffer
    ConstantBufferSceneMesh temp_mesh_data =
    {
        instance_offset
    };
        
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSceneMesh>()->UploadAndApplyDataWithIndex(resource_manager, instance_offset, temp_mesh_data, true))
    
    return true;   
}

bool glTFGraphicsPassMeshBase::EndDrawMesh(glTFRenderResourceManager& resource_manager, const glTFMeshRenderResource& mesh_data, unsigned mesh_index)
{
    return true;
}

bool glTFGraphicsPassMeshBase::InitVertexAndInstanceBufferForFrame(glTFRenderResourceManager& resource_manager)
{
    if (m_instance_buffer_view)
    {
        return true;
    }
    
    // Render meshes
    const auto& mesh_manager = resource_manager.GetMeshManager();
    
    const auto& mesh_render_resources = mesh_manager.GetMeshRenderResources();
    const auto& mesh_instance_render_resource = mesh_manager.GetMeshInstanceRenderResource();
    
    VertexBufferData instance_buffer;
    instance_buffer.layout.elements = m_instance_input_layout.m_instance_layout.elements;
    instance_buffer.byteSize = mesh_instance_render_resource.size() * sizeof(MeshInstanceInputData);
    instance_buffer.vertex_count = mesh_instance_render_resource.size();
    
    instance_buffer.data = std::make_unique<char[]>(instance_buffer.byteSize);
    std::vector<MeshInstanceInputData> instance_datas; instance_datas.reserve(mesh_instance_render_resource.size());
    
    for (const auto& mesh : mesh_render_resources)
    {
        auto mesh_ID = mesh.first;
        
        std::vector<glTFUniqueID> instances;
        // Find all instances with current mesh id
        for (const auto& instance : mesh_instance_render_resource)
        {
            if (instance.second.m_mesh_render_resource == mesh_ID)
            {
                instances.push_back(instance.first);
            }
        }

        m_instance_draw_infos[mesh_ID] = {instances.size(), instance_datas.size()};
        
        for (size_t instance_index = 0; instance_index < instances.size(); ++instance_index)
        {
            auto instance_data = mesh_instance_render_resource.find(instances[instance_index]);
            GLTF_CHECK(instance_data != mesh_instance_render_resource.end());
            
            instance_datas.emplace_back( MeshInstanceInputData
                {
                    instance_data->second.m_instance_transform,
                    instance_data->second.m_instance_material_id,
                    instance_data->second.m_normal_mapping ? 1u : 0u,
                    mesh_ID, 0
                });
        }
    }

    memcpy(instance_buffer.data.get(), instance_datas.data(), instance_buffer.byteSize);
    
    const RHIBufferDesc instance_buffer_desc = {L"instanceBufferDefaultBuffer", instance_buffer.byteSize, 1, 1, RHIBufferType::Default, RHIDataFormat::Unknown, RHIBufferResourceType::Buffer};
    m_instance_buffer = RHIResourceFactory::CreateRHIResource<IRHIVertexBuffer>();
    m_instance_buffer_view = m_instance_buffer->CreateVertexBufferView(resource_manager.GetDevice(), resource_manager.GetCommandListForRecord(), instance_buffer_desc, instance_buffer);

    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceStructuredBuffer<MeshInstanceInputData>>()->UploadCPUBuffer(
               instance_datas.data(), 0, instance_datas.size() * sizeof(MeshInstanceInputData)))
    
    return true;
}

std::vector<RHIPipelineInputLayout> glTFGraphicsPassMeshBase::GetVertexInputLayout()
{
    return m_vertex_input_layouts;
}

bool glTFGraphicsPassMeshBase::TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object)
{
    return true;
}

bool glTFGraphicsPassMeshBase::ResolveVertexInputLayout(const VertexLayoutDeclaration& source_vertex_layout)
{
	m_vertex_input_layouts.clear();
    
    unsigned vertex_layout_offset = 0;
    for (const auto& vertex_layout : source_vertex_layout.elements)
    {
        switch (vertex_layout.type)
        {
        case VertexLayoutType::POSITION:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32B32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(POSITION), 0, RHIDataFormat::R32G32B32_FLOAT, vertex_layout_offset, 0});
            }
            break;
        case VertexLayoutType::NORMAL:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32B32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(NORMAL), 0, RHIDataFormat::R32G32B32_FLOAT, vertex_layout_offset, 0});
            }
            break;
        case VertexLayoutType::TANGENT:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32B32A32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(TANGENT), 0, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 0});
            }
            break;
        case VertexLayoutType::TEXCOORD_0:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetRHIDataFormatBitsPerPixel(RHIDataFormat::R32G32_FLOAT) / 8));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(TEXCOORD), 0, RHIDataFormat::R32G32_FLOAT, vertex_layout_offset, 0});
            }
            break;
            // TODO: Handle TEXCOORD_1?
        }

        vertex_layout_offset += vertex_layout.byte_size;   
    }

    GLTF_CHECK(m_instance_input_layout.ResolveInputInstanceLayout(m_vertex_input_layouts));
    
    return true;
}