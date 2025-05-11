#pragma once
#include <map>
#include <queue>
#include <set>

#include "VTPageDataAccessor.h"
#include "glTFRenderPass/glTFRenderPassBase.h"

class glTFGraphicsPassMeshRVTPageRendering;
class glTFGraphicsPassMeshVSMPageRendering;
class glTFRenderPassManager;
class glTFComputePassVTFetchCS;
class glTFGraphicsPassMeshVTFeedbackBase;

// Size -- width and height
struct VTLogicalTextureConfig
{
    unsigned virtual_texture_id;
    bool isSVT;
};

enum class LogicalTextureType
{
    SVT,
    RVT,
};

struct RVTPageRenderingInfo
{
    float mip_page_size;
    float page_x;
    float page_y;
    
    int physical_page_size;
    int physical_page_x;
    int physical_page_y;

    std::string ToString() const
    {
        return std::format("RVT_PAGE_INFO_[PHYSICAL_{0}_{1}]_[LOGICAL_{2}_{3}]", physical_page_x, physical_page_y, page_x, page_y);
    }
};

class VTLogicalTextureBase
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VTLogicalTextureBase)
    
    bool InitLogicalTexture(const RHITextureDesc& desc, const VTLogicalTextureConfig& config);
    
    virtual bool InitRenderResource(glTFRenderResourceManager& resource_manager);
    virtual bool SetupPassManager(glTFRenderPassManager& pass_manager) const;
    virtual void UpdateRenderResource(glTFRenderResourceManager& resource_manager);
    
    int GetTextureId() const;
    int GetSize() const;
    
    virtual bool GetPageData(const VTPage& page, VTPageData& out) const = 0;
    VTPageTable& GetPageTable() const;
    
    glTFRenderPassBase& GetFeedbackPass() const;

    void SetEnableGatherRequest(bool enable);

    virtual LogicalTextureType GetLogicalTextureType() const = 0;
    
protected:
    virtual bool GeneratePageData() = 0;
    void DumpGeneratedPageDataToFile() const;

    bool m_render_resource_init {false};
    bool m_gather_request_enabled {false};
    
    int m_texture_id {-1};
    int m_logical_texture_width {-1};
    int m_logical_texture_height {-1};

    std::shared_ptr<unsigned char[]> m_texture_data {nullptr};
    RHIDataFormat m_texture_format {RHIDataFormat::UNKNOWN};
    size_t m_texture_data_size {0};

    std::map<VTPage::HashType, VTPageData> m_page_data;
    std::shared_ptr<VTPageTable> m_page_table;

    std::shared_ptr<glTFRenderPassBase> m_feedback_pass;
};

class VTLogicalTextureSVT : public VTLogicalTextureBase
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VTLogicalTextureSVT)

    virtual LogicalTextureType GetLogicalTextureType() const override {return LogicalTextureType::SVT;}
    virtual bool InitRenderResource(glTFRenderResourceManager& resource_manager) override;
    virtual bool GetPageData(const VTPage& page, VTPageData& out) const override;

protected:    
    virtual bool GeneratePageData() override;
};

class VTLogicalTextureRVT : public VTLogicalTextureBase
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VTLogicalTextureRVT)

    virtual bool SetupPassManager(glTFRenderPassManager& pass_manager) const override;
    virtual void UpdateRenderResource(glTFRenderResourceManager& resource_manager);

    virtual LogicalTextureType GetLogicalTextureType() const override {return LogicalTextureType::RVT;}
    virtual bool InitRenderResource(glTFRenderResourceManager& resource_manager) override;
    virtual bool GetPageData(const VTPage& page, VTPageData& out) const override;

    void EnqueuePageRenderingRequest(const std::vector<RVTPageRenderingInfo>& requests);
    
protected:
    virtual bool GeneratePageData() override;

    virtual std::shared_ptr<glTFGraphicsPassMeshRVTPageRendering> GetRVTPass() const = 0;

    std::vector<RVTPageRenderingInfo> m_pending_page_rendering_requests;
};

class VTShadowmapLogicalTexture : public VTLogicalTextureRVT
{
public:
    VTShadowmapLogicalTexture(unsigned light_id);
    DECLARE_NON_COPYABLE_AND_VDTOR(VTShadowmapLogicalTexture)

    virtual bool InitRenderResource(glTFRenderResourceManager& resource_manager) override;

    unsigned GetLightId() const;

protected:
    virtual std::shared_ptr<glTFGraphicsPassMeshRVTPageRendering> GetRVTPass() const override;
    
    unsigned m_light_id {0};
    std::shared_ptr<glTFGraphicsPassMeshVSMPageRendering> m_vsm_rvt_pass;
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
    void InsertPage(const std::vector<VTPageData>& pages_to_insert);

    bool InitRenderResource(glTFRenderResourceManager& resource_manager);
    void UpdateRenderResource(glTFRenderResourceManager& resource_manager);

    const std::map<VTPage::HashType, VTPhysicalPageAllocationInfo>& GetPageAllocations() const;
    std::shared_ptr<IRHITextureAllocation> GetTextureAllocation() const;

    bool IsSVT() const;
    unsigned GetPageCapacity() const;

    void RegisterLogicalTexture(std::shared_ptr<VTLogicalTextureBase> logical_texture);
    bool HasPendingStreamingPages() const;
    
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
    std::set<VTPage::HashType> m_pending_streaming_pages;
    VTPageLRU m_page_lru_cache;

    std::shared_ptr<IRHITextureAllocation> m_physical_texture;
    std::shared_ptr<VTTextureData> m_physical_texture_data;

    std::map<unsigned, std::shared_ptr<VTLogicalTextureBase>> m_logical_textures;
};
