#pragma once

#include <map>
#include <memory>

#include "../glTFMaterial/glTFMaterialBase.h"
#include "../glTFRHI/RHIInterface/IRHITexture.h"
#include "../glTFUtils/glTFUtils.h"

class glTFMaterialBase;
class IRHIDescriptorHeap;
class glTFRenderResourceManager;
class glTFMaterialParameterTexture;

class glTFMaterialTextureRenderResource
{
public:
	glTFMaterialTextureRenderResource(const glTFMaterialParameterTexture& material);

	bool Init(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& descriptor_heap);
	RHICPUDescriptorHandle GetTextureSRVHandle() const;
    
private:
	const glTFMaterialParameterTexture& m_source_texture;
	std::shared_ptr<IRHITexture> m_texture_buffer;
	RHICPUDescriptorHandle m_texture_SRV_handle; 
};

class glTFMaterialRenderResource
{
public:
	glTFMaterialRenderResource(const glTFMaterialBase& source_texture);

protected:
	const glTFMaterialBase& m_source_material;
};

class glTFRenderMaterialManager
{
public:
	void AddMaterial(const glTFMaterialBase& material);

protected:
	std::map<glTFUniqueID, glTFMaterialRenderResource> m_materials;
	std::map<glTFUniqueID, glTFMaterialTextureRenderResource> m_textures;
};
