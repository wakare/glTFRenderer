#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "RHIUtils.h"
#include "RHIInterface/IRHIDescriptorManager.h"
#include "RHIInterface/IRHIDescriptorUpdater.h"
#include "RHIInterface/IRHIRootSignatureHelper.h"

template<unsigned TableRangeCount, RHIDescriptorRangeType TextureType>
class glTFRenderInterfaceTextureTable : public glTFRenderInterfaceWithRSAllocation
{
public:
    glTFRenderInterfaceTextureTable(const char* name)
        : glTFRenderInterfaceWithRSAllocation(name)
    {
    }

    virtual bool PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager) override
    {
        if (m_textures.empty())
        {
            return true;
        }
        
        if (!m_descriptor_table)
        {
            m_descriptor_table = CreateDescriptorTable();
            for (const auto& texture : m_textures)
            {
                std::shared_ptr<IRHITextureDescriptorAllocation> result;
                if (TextureType == RHIDescriptorRangeType::SRV)
                {
                    RHIDataFormat format = texture->GetTextureDesc().GetDataFormat() == RHIDataFormat::R32_TYPELESS ? RHIDataFormat::D32_SAMPLE_RESERVED : texture->GetTextureDesc().GetDataFormat();
                    resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), texture,
                            RHITextureDescriptorDesc{
                                format,
                                RHIResourceDimension::TEXTURE2D,
                                RHIViewType::RVT_SRV,
                            },
                            result);    
                }
                else if (TextureType == RHIDescriptorRangeType::UAV)
                {
                    resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), texture,
                            RHITextureDescriptorDesc{
                                texture->GetTextureDesc().GetDataFormat(),
                                RHIResourceDimension::TEXTURE2D,
                                RHIViewType::RVT_UAV,
                            },
                            result);
                }
                
                m_texture_descriptors.push_back(result);
            }
            
            bool succeed = m_descriptor_table->Build(resource_manager.GetDevice(), m_texture_descriptors);
            GLTF_CHECK(succeed);
        }
        
        return true;
    }
    
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override
    {
        return root_signature.AddTableRootParameter(m_name, {TextureType, TableRangeCount, false, TableRangeCount == UINT_MAX}, m_allocation);
    }

    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned
                                    frame_index) override
    {
        if (m_descriptor_table)
        {
            // Important: Must do transition because DX may do transition when bind SRV table 
            for (const auto& entry : m_texture_descriptors)
            {
                auto& texture = entry->m_source;
                
                if (TextureType== RHIDescriptorRangeType::SRV)
                {
                    texture->Transition(command_list, RHIResourceStateType::STATE_ALL_SHADER_RESOURCE);    
                }
                else if (TextureType == RHIDescriptorRangeType::UAV)
                {
                    texture->Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);
                }
            }
            descriptor_updater.BindTextureDescriptorTable(command_list, pipeline_type, GetRSAllocation(), *m_descriptor_table, TextureType);
        }
        
        return true;
    }
    
    void AddTexture(const std::vector<std::shared_ptr<IRHITexture>>& textures)
    {
        for (const auto& texture : textures)
        {
            if (!texture)
            {
                GLTF_CHECK(false);
                return;
            }
        }
        
        m_textures.insert(m_textures.end(), textures.begin(), textures.end());
    }
    
protected:
    std::vector<std::shared_ptr<IRHITexture>> m_textures;
    std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> m_texture_descriptors;
    
    std::shared_ptr<IRHIDescriptorTable> m_descriptor_table;
};

// Bindless specific situation
template<RHIDescriptorRangeType TextureType>
class glTFRenderInterfaceTextureTableBindless : public glTFRenderInterfaceTextureTable<UINT_MAX, TextureType>
{
public:
    glTFRenderInterfaceTextureTableBindless(const char* name)
        : glTFRenderInterfaceTextureTable<UINT_MAX, TextureType>(name)
    {}
};