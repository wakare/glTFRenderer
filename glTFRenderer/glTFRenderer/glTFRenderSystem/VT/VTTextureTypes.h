#pragma once
#include <map>
#include <queue>
#include <set>

#include "VTPageDataAccessor.h"

// Size -- width and height
struct VTLogicalTextureConfig
{
    unsigned virtual_texture_id;
    bool isSVT;
};

class VTLogicalTexture
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VTLogicalTexture)
    
    bool InitLogicalTexture(const RHITextureDesc& desc, const VTLogicalTextureConfig& config);
    
    bool InitRenderResource(glTFRenderResourceManager& resource_manager);
    void UpdateRenderResource(glTFRenderResourceManager& resource_manager);
    
    int GetTextureId() const;
    int GetSize() const;
    
    bool GetPageData(const VTPage& page, VTPageData& out) const;

    bool IsSVT() const;
    
protected:
    bool GeneratePageData();
    void DumpGeneratedPageDataToFile() const;

    bool m_svt{true};
    
    bool m_render_resource_init{false};
    int m_texture_id {-1};
    int m_logical_texture_width {-1};
    int m_logical_texture_height {-1};

    std::shared_ptr<unsigned char[]> m_texture_data {nullptr};
    RHIDataFormat m_texture_format{RHIDataFormat::UNKNOWN};
    size_t m_texture_data_size {0};

    std::map<VTPage::HashType, VTPageData> m_page_data;
};

class VTShadowmapLogicalTexture : public VTLogicalTexture
{
public:
    VTShadowmapLogicalTexture(unsigned light_id);
    DECLARE_NON_COPYABLE_AND_VDTOR(VTShadowmapLogicalTexture)

    unsigned GetLightId() const;
    
protected:
    unsigned m_light_id {0};
};

class VTPageLRU
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VTPageLRU)

    void AddPage(const VTPage& page);
    void RemovePage(const VTPage& page);
    void TouchPage(const VTPage& page);

    const VTPage& GetPageForFree() const;
    
protected:
    std::set<VTPage> m_pages;
    std::list<VTPage> m_lru_pages;
};

class VTPhysicalTexture
{
public:
    VTPhysicalTexture(int texture_size, int page_size, int border, bool svt);
    void ProcessRequestResult(const std::vector<VTPageData>& results);
    void UpdateTextureData();

    bool InitRenderResource(glTFRenderResourceManager& resource_manager);
    void UpdateRenderResource(glTFRenderResourceManager& resource_manager);
    
    const std::map<VTPage::HashType, VTPhysicalPageAllocationInfo>& GetPageAllocationInfos() const;
    std::shared_ptr<IRHITextureAllocation> GetTextureAllocation() const;
    
protected:
    bool GetAvailablePagesAndErase(int& x, int& y);

    bool m_render_resource_init {false};
    
    int m_texture_size{0};
    int m_page_table_size{0};
    int m_page_size{0};
    int m_border{0};
    bool m_svt{true};

    std::vector<std::pair<int, int>> m_available_pages;
    std::map<VTPage::HashType, VTPhysicalPageAllocationInfo> m_page_allocations;
    VTPageLRU m_page_lru_cache;

    std::shared_ptr<IRHITextureAllocation> m_physical_texture;
    std::shared_ptr<VTTextureData> m_physical_texture_data;
};