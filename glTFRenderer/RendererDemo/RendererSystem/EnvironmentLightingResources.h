#pragma once

#include "RenderPassSetupBuilder.h"
#include "RendererInterface.h"

#include <array>
#include <string>
#include <vector>

class EnvironmentLightingResources
{
public:
    static constexpr const char* kDefaultTextureUri = "glTFResources/Models/Plane/dawn_4k.png";
    static constexpr std::size_t kPrefilterLevelCount = 4u;

    bool Init(RendererInterface::ResourceOperator& resource_operator);
    void Reset();

    bool IsReady() const;
    bool HasTextureSource() const;
    const std::string& GetSourceTextureUri() const;
    glm::fvec4 GetPrefilterRoughnessParams() const;
    const std::array<float, kPrefilterLevelCount>& GetPrefilterRoughnessLevels() const;
    void AddLightingPassBindings(RenderFeature::PassBuilder& builder) const;

private:
    bool HasPrefilterTexture() const;
    void EnsureEnvironmentTextureSet(RendererInterface::ResourceOperator& resource_operator);
    void EnsureEnvironmentBrdfLut(RendererInterface::ResourceOperator& resource_operator);
    void BuildTexturesFromSource(RendererInterface::ResourceOperator& resource_operator);
    void CreateFallbackTextureSet(RendererInterface::ResourceOperator& resource_operator);

    std::string m_environment_texture_uri{kDefaultTextureUri};
    RendererInterface::TextureHandle m_environment_texture_handle{NULL_HANDLE};
    RendererInterface::TextureHandle m_environment_irradiance_texture_handle{NULL_HANDLE};
    RendererInterface::TextureHandle m_environment_prefilter_texture_handle{NULL_HANDLE};
    RendererInterface::TextureHandle m_environment_brdf_lut_texture_handle{NULL_HANDLE};
    bool m_has_texture_source{false};
};
