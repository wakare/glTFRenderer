#pragma once

#include <map>
#include <memory>

#include "glTFRenderPassBase.h"
#include "../glTFMaterial/glTFMaterialBase.h"
#include "../glTFRHI/RHIInterface/IRHITexture.h"
#include "../glTFUtils/glTFUtils.h"

enum class glTFMaterialParameterUsage;
class glTFMaterialBase;
class IRHIDescriptorHeap;
class glTFRenderResourceManager;
class glTFMaterialParameterTexture;

class glTFMaterialTextureRenderResource
{
public:
	glTFMaterialTextureRenderResource(const glTFMaterialParameterTexture& source_texture);

	bool Init(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap);
	RHIGPUDescriptorHandle CreateOrGetTextureSRVHandle(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap);
    
private:
	const glTFMaterialParameterTexture& m_source_texture;
	std::shared_ptr<IRHITexture> m_texture_buffer;
	RHIGPUDescriptorHandle m_texture_SRV_handle; 
};

class glTFMaterialRenderResource
{
public:
	glTFMaterialRenderResource(const glTFMaterialBase& source_material);
	bool Init(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap);
	
	
	RHIGPUDescriptorHandle CreateOrGetAllTextureFirstGPUHandle(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap);
	
protected:
	const glTFMaterialBase& m_source_material;
	std::map<glTFMaterialParameterUsage, std::unique_ptr<glTFMaterialTextureRenderResource>> m_textures;
	
};

class glTFRenderMaterialManager
{
public:
	bool InitMaterialRenderResource(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap, const glTFMaterialBase& material);
	bool ApplyMaterialRenderResource(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap, glTFUniqueID material_ID, unsigned slot_index);
	
protected:
	std::map<glTFUniqueID, std::unique_ptr<glTFMaterialRenderResource>> m_material_render_resources;
};