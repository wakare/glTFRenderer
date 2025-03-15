#include "glTFComputePassIndirectDrawCulling.h"

#include <imgui.h>

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFScene/glTFSceneView.h"

ALIGN_FOR_CBV_STRUCT struct CullingConstant
{
    inline static std::string Name = "CULLING_CONSTANT_BUFFER_REGISTER_CBV_INDEX";
    
    unsigned command_count;
};

ALIGN_FOR_CBV_STRUCT struct CullingBoundingBox
{
    inline static std::string Name = "CULLING_BOUNDING_BOX_REGISTER_SRV_INDEX";

    glm::vec4 bounding_box;
};

glTFComputePassIndirectDrawCulling::glTFComputePassIndirectDrawCulling()
    : m_dispatch_count()
    , m_enable_culling(false)
{
}

const char* glTFComputePassIndirectDrawCulling::PassName()
{
    return "IndirectDrawCullingComputePass";
}

bool glTFComputePassIndirectDrawCulling::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitRenderInterface(resource_manager))

    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<MeshInstanceInputData>>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<MeshIndirectDrawCommand>>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<CullingConstant>>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<CullingBoundingBox>>());

    return true;
}

bool glTFComputePassIndirectDrawCulling::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitPass(resource_manager))

    const auto& mesh_manager = resource_manager.GetMeshManager();

    const char* cached_data;
    size_t cached_data_size;
    mesh_manager.GetIndirectDrawBuilder().GetCachedData(cached_data, cached_data_size);
    
    RHIUAVStructuredBufferDesc uav_structured_buffer_desc
    {
        sizeof(MeshIndirectDrawCommand),
        (mesh_manager.GetIndirectDrawBuilder().GetCachedCommandCount()),
    };
    
    RHIBufferDescriptorDesc uav_structured_buffer_descriptor_desc
    {
        RHIDataFormat::UNKNOWN,
        RHIViewType::RVT_UAV,
        static_cast<unsigned>(cached_data_size),
        0,
        uav_structured_buffer_desc
    };
    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
                resource_manager.GetDevice(),
                mesh_manager.GetIndirectDrawBuilder().GetCulledIndirectArgumentBuffer(),
                uav_structured_buffer_descriptor_desc,
                m_culled_command_output_descriptor))

    RHIUAVStructuredBufferDesc count_buffer_desc
    {
        sizeof(unsigned),
        1,
        false,
    };
    RHIBufferDescriptorDesc count_buffer_descriptor_desc
    {
        RHIDataFormat::R32_UINT,
        RHIViewType::RVT_UAV,
        static_cast<unsigned>(mesh_manager.GetIndirectDrawBuilder().GetCountBuffer()->GetBufferDesc().width),
        0,
        count_buffer_desc
    };

    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
                resource_manager.GetDevice(),
                mesh_manager.GetIndirectDrawBuilder().GetCountBuffer(),
                count_buffer_descriptor_desc,
                m_culled_count_output_descriptor))
    

    const unsigned dispatch_thread = static_cast<unsigned>(ceil(mesh_manager.GetIndirectDrawBuilder().GetCachedCommandCount() / 64.0f));
    m_dispatch_count = {dispatch_thread, 1, 1};

    mesh_manager.GetIndirectDrawBuilder().GetCachedData(cached_data, cached_data_size);
    
    const auto& instance_data = mesh_manager.GetInstanceBufferData();
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<MeshInstanceInputData>>()->UploadBuffer(resource_manager, instance_data.data(), 0, instance_data.size() * sizeof(MeshInstanceInputData));
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<MeshIndirectDrawCommand>>()->UploadBuffer(resource_manager, cached_data, 0, cached_data_size);

    // Construct bounding box srv data
    std::vector<CullingBoundingBox> bounding_boxes;
    const auto& mesh_data = resource_manager.GetMeshManager().GetMeshRenderResources();

    for (unsigned i = 0; i < mesh_data.size(); ++i)
    {
        const auto mesh_iter = mesh_data.find(i);
        GLTF_CHECK(mesh_iter != mesh_data.end());

        const auto& AABB = mesh_iter->second.mesh->GetAABB();
        const auto& center = AABB.getCenter();
        const float radius = length(AABB.getDiagonal()) * 0.5f;
        bounding_boxes.push_back({glm::vec4{center.x,center.y,center.z, radius }});
    }
    
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<CullingBoundingBox>>()->UploadBuffer(resource_manager, bounding_boxes.data(), 0, bounding_boxes.size() * sizeof(CullingBoundingBox));
    
    return true;
}

bool glTFComputePassIndirectDrawCulling::SetupRootSignature(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupRootSignature(resourceManager))

    RETURN_IF_FALSE(m_root_signature_helper.AddUAVRootParameter("INDIRECT_DRAW_DATA_OUTPUT_REGISTER_UAV_INDEX", m_culled_indirect_command_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddUAVRootParameter("INDIRECT_COMMAND_COUNT_BUFFER_REGISTER_UAV_INDEX",  m_culled_count_output_allocation))

    return true;
}

bool glTFComputePassIndirectDrawCulling::SetupPipelineStateObject(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resourceManager))

    GetComputePipelineStateObject().BindShaderCode(
            R"(glTFResources\ShaderSource\ComputeShader\IndirectCullingCS.hlsl)", RHIShaderType::Compute, "main");
    
    auto& shaderMacros = GetComputePipelineStateObject().GetShaderMacros();
    m_culled_indirect_command_allocation.AddShaderDefine(shaderMacros);
    m_culled_count_output_allocation.AddShaderDefine(shaderMacros);
    return true;
}

bool glTFComputePassIndirectDrawCulling::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))
    if (m_enable_culling)
    {
        auto& command_list = resource_manager.GetCommandListForRecord();
        auto& indirect_argument_buffer = *resource_manager.GetMeshManager().GetIndirectDrawBuilder().GetCulledIndirectArgumentBuffer();
        indirect_argument_buffer.Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS );

        const char* cached_data;
        size_t cached_data_size;
        resource_manager.GetMeshManager().GetIndirectDrawBuilder().GetCachedData(cached_data, cached_data_size);
        GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<CullingConstant>>()->UploadBuffer(resource_manager, &cached_data_size, 0, sizeof(unsigned));
        
        BindDescriptor(command_list,
            m_culled_indirect_command_allocation,
            *m_culled_command_output_descriptor);
        
        BindDescriptor(command_list,
            m_culled_count_output_allocation,
            *m_culled_count_output_descriptor);
    }
    
    return true;
}

bool glTFComputePassIndirectDrawCulling::IsRenderingEnabled() const
{
    return m_enable_culling;
}

DispatchCount glTFComputePassIndirectDrawCulling::GetDispatchCount(glTFRenderResourceManager& resource_manager) const
{
    return m_dispatch_count;
}

bool glTFComputePassIndirectDrawCulling::UpdateGUIWidgets()
{
    RETURN_IF_FALSE(glTFComputePassBase::UpdateGUIWidgets())

    ImGui::Checkbox("Enable Culling", &m_enable_culling);
    
    return true;
}
