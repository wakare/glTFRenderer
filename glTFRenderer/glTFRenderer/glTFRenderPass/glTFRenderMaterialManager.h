#pragma once

#include <map>
#include <memory>

#include "glTFMaterial/glTFMaterialParameterFactor.h"
#include "RendererCommon.h"
#include "glTFRenderSystem/VT/VTTextureTypes.h"

enum class glTFMaterialParameterUsage;
class IRHITextureAllocation;
class glTFMaterialBase;
class glTFRenderResourceManager;
class glTFMaterialParameterTexture;

class glTFMaterialTextureRenderResource
{
public:
	enum
	{
		VT_TEXTURE_SIZE = 4096,
	};
	
	glTFMaterialTextureRenderResource(const glTFMaterialParameterTexture& source_texture);

	bool Init(glTFRenderResourceManager& resource_manager);

	bool IsVT() const;
	std::shared_ptr<VTLogicalTexture> GetVTTexture() const;
	const IRHITextureAllocation& GetTextureAllocation() const;
	
private:
	bool m_vt {false};
	
	const glTFMaterialParameterTexture& m_source_texture;
	std::shared_ptr<IRHITextureAllocation> m_texture;
	std::shared_ptr<VTLogicalTexture> m_virtual_texture;
};

class glTFMaterialRenderResource
{
public:
	enum
	{
		VT_TEXTURE_SIZE = 4096,
	};
	
	glTFMaterialRenderResource(const glTFMaterialBase& source_material);
	bool Init(glTFRenderResourceManager& resource_manager);
	
	const std::map<glTFMaterialParameterUsage, std::unique_ptr<glTFMaterialTextureRenderResource>>& GetTextures() const;
	const std::map<glTFMaterialParameterUsage, std::unique_ptr<glTFMaterialParameterFactor<glm::vec4>>>& GetFactors() const;
	const std::map<glTFMaterialParameterUsage, std::unique_ptr<VTLogicalTexture>>& GetVirtualTextures() const;
	
protected:
	std::map<glTFMaterialParameterUsage, std::unique_ptr<glTFMaterialTextureRenderResource>> m_textures;
	std::map<glTFMaterialParameterUsage, std::unique_ptr<glTFMaterialParameterFactor<glm::vec4>>> m_factors;
	std::map<glTFMaterialParameterUsage, std::unique_ptr<VTLogicalTexture>> m_virtual_textures;
};

class glTFRenderMaterialManager
{
public:
	bool AddMaterialRenderResource(glTFRenderResourceManager& resource_manager, const glTFMaterialBase& material);
	const std::map<glTFUniqueID, std::unique_ptr<glTFMaterialRenderResource>>& GetMaterialRenderResources() const;
	
protected:
	std::map<glTFUniqueID, std::unique_ptr<glTFMaterialRenderResource>> m_material_render_resources;
};