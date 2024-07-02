#include "glTFComputePassIndirectDrawCulling.h"

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
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<MeshInstanceInputData>>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<MeshIndirectDrawCommand>>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<CullingConstant>>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<CullingBoundingBox>>());
}

const char* glTFComputePassIndirectDrawCulling::PassName()
{
    return "IndirectDrawCullingComputePass";
}

bool glTFComputePassIndirectDrawCulling::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitPass(resource_manager))

    const auto& mesh_manager = resource_manager.GetMeshManager();
        
    RETURN_IF_FALSE(MainDescriptorHeapRef().CreateResourceDescriptorInHeap(
            resource_manager.GetDevice(),
            *mesh_manager.GetCulledIndirectArgumentBuffer(),
            {
            RHIDataFormat::Unknown,
            RHIResourceDimension::BUFFER,
            RHIViewType::RVT_UAV,
            sizeof(MeshIndirectDrawCommand),
            static_cast<unsigned>(mesh_manager.GetIndirectDrawCommands().size()),
            true,
            mesh_manager.GetCulledIndirectArgumentBufferCountOffset()
            },
            m_command_buffer_handle))

    const unsigned dispatch_thread = static_cast<unsigned>(ceil(mesh_manager.GetIndirectDrawCommands().size() / 64.0f));
    m_dispatch_count = {dispatch_thread, 1, 1};

    const auto& instance_data = mesh_manager.GetInstanceBufferData();
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<MeshInstanceInputData>>()->UploadCPUBuffer(instance_data.data(), 0, instance_data.size() * sizeof(MeshInstanceInputData));

    const auto& indirect_data = mesh_manager.GetIndirectDrawCommands();
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<MeshIndirectDrawCommand>>()->UploadCPUBuffer(indirect_data.data(), 0, indirect_data.size() * sizeof(MeshIndirectDrawCommand));

    glTFRenderResourceManager::GetMemoryManager().AllocateBufferMemory(
        glTFRenderResourceManager::GetDevice(),
    {L"ResetBuffer",
        4,
        1,
        1,
        RHIBufferType::Upload,
        RHIDataFormat::R32_UINT,
        RHIBufferResourceType::Buffer,
        RHIResourceStateType::STATE_COMMON,
        },
        m_count_reset_buffer);
    const unsigned size = 0;
    glTFRenderResourceManager::GetMemoryManager().UploadBufferData(*m_count_reset_buffer, &size, 0, sizeof(unsigned));

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
    
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<CullingBoundingBox>>()->UploadCPUBuffer(bounding_boxes.data(), 0, bounding_boxes.size() * sizeof(CullingBoundingBox));
    
    return true;
}

bool glTFComputePassIndirectDrawCulling::SetupRootSignature(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupRootSignature(resourceManager))

    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("Output", RHIRootParameterDescriptorRangeType::UAV, 1, false, m_culled_indirect_command_allocation))

    return true;
}

bool glTFComputePassIndirectDrawCulling::SetupPipelineStateObject(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resourceManager))

    GetComputePipelineStateObject().BindShaderCode(
            R"(glTFResources\ShaderSource\ComputeShader\IndirectCullingCS.hlsl)", RHIShaderType::Compute, "main");
    
    auto& shaderMacros = GetComputePipelineStateObject().GetShaderMacros();
    shaderMacros.AddUAVRegisterDefine("INDIRECT_DRAW_DATA_OUTPUT_REGISTER_UAV_INDEX", m_culled_indirect_command_allocation.register_index, m_culled_indirect_command_allocation.space);
    
    return true;
}

bool glTFComputePassIndirectDrawCulling::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))
    if (enable_culling)
    {
        auto& command_list = resource_manager.GetCommandListForRecord();
        RHIUtils::Instance().SetDTToRootParameterSlot(command_list,
            m_culled_indirect_command_allocation.parameter_index,
            *m_command_buffer_handle,
            GetPipelineType() == PipelineType::Graphics);

        RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, *resource_manager.GetMeshManager().GetCulledIndirectArgumentBuffer(),
            RHIResourceStateType::STATE_UNORDERED_ACCESS,RHIResourceStateType::STATE_COPY_DEST );
    
        // Reset count buffer to zero
        RHIUtils::Instance().CopyBuffer(command_list, *resource_manager.GetMeshManager().GetCulledIndirectArgumentBuffer(),
            resource_manager.GetMeshManager().GetCulledIndirectArgumentBufferCountOffset(), *m_count_reset_buffer->m_buffer, 0, sizeof(unsigned));

        RHIUtils::Instance().AddBufferBarrierToCommandList(command_list, *resource_manager.GetMeshManager().GetCulledIndirectArgumentBuffer(),
                RHIResourceStateType::STATE_COPY_DEST, RHIResourceStateType::STATE_UNORDERED_ACCESS);

        const unsigned command_count = resource_manager.GetMeshManager().GetIndirectDrawCommands().size();
        GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<CullingConstant>>()->UploadCPUBuffer(&command_count, 0, sizeof(unsigned));
    }
    
    return true;
}

void glTFComputePassIndirectDrawCulling::UpdateRenderFlags(const glTFPassOptionRenderFlags& render_flags)
{
    if (enable_culling != render_flags.IsCulling())
    {
        enable_culling = render_flags.IsCulling();
        LOG_FORMAT_FLUSH("[DEBUG] Enable Culling: %s\n", enable_culling ? "True" : "False")
    }
}

bool glTFComputePassIndirectDrawCulling::NeedRendering() const
{
    return enable_culling;
}

DispatchCount glTFComputePassIndirectDrawCulling::GetDispatchCount() const
{
    return m_dispatch_count;
}

size_t glTFComputePassIndirectDrawCulling::GetMainDescriptorHeapSize()
{
    return 64;
}
