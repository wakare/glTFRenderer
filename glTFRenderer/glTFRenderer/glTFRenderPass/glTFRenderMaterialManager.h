#pragma once

#include <map>
#include <memory>

#include "glTFMaterial/glTFMaterialParameterFactor.h"
#include "glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "RendererCommon.h"

enum class glTFMaterialParameterUsage;
class IRHITextureAllocation;
class glTFMaterialBase;
class glTFRenderResourceManager;
class glTFMaterialParameterTexture;

class glTFMaterialTextureRenderResource
{
public:
	glTFMaterialTextureRenderResource(const glTFMaterialParameterTexture& source_texture);

	bool Init(glTFRenderResourceManager& resource_manager);
	
	const IRHITextureAllocation& GetTextureAllocation() const;
	
private:
	const glTFMaterialParameterTexture& m_source_texture;
	std::shared_ptr<IRHITextureAllocation> m_texture;
};

class glTFMaterialRenderResource
{
public:
	glTFMaterialRenderResource(const glTFMaterialBase& source_material);
	bool Init(glTFRenderResourceManager& resource_manager);
	
	const std::map<glTFMaterialParameterUsage, std::unique_ptr<glTFMaterialTextureRenderResource>>& GetTextures() const;
	const std::map<glTFMaterialParameterUsage, std::unique_ptr<glTFMaterialParameterFactor<glm::vec4>>>& GetFactors() const;
	
protected:
	const glTFMaterialBase& m_source_material;
	std::map<glTFMaterialParameterUsage, std::unique_ptr<glTFMaterialTextureRenderResource>> m_textures;
	std::map<glTFMaterialParameterUsage, std::unique_ptr<glTFMaterialParameterFactor<glm::vec4>>> m_factors;
	
};

class glTFRenderMaterialManager
{
public:
	bool InitMaterialRenderResource(glTFRenderResourceManager& resource_manager, const glTFMaterialBase& material);
	
	bool GatherAllMaterialRenderResource(
		std::vector<MaterialInfo>& gather_material_infos, std::vector<glTFMaterialTextureRenderResource*>&
		gather_material_textures) const;
	
protected:
	std::map<glTFUniqueID, std::unique_ptr<glTFMaterialRenderResource>> m_material_render_resources;
};