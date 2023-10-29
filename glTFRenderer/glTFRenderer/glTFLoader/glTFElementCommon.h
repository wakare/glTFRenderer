#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "glm.hpp"
#include "glTFUtils/glTFUtils.h"

enum glTF_Element_Type
{
    EScene,
    ENode,
    EMesh,
    EAccessor,
    EMaterial,
    EBufferView,
    EBuffer,
    ETexture,
    ESampler,
    EImage,
    ECamera,
    ESkin,
    EAnimation,
    EAsset,
};

struct glTFHandle
{
    using HandleIndexType = int;
    using HandleNameType = std::string;
    enum
    {
        glTF_ELEMENT_INVALID_HANDLE = INT_MAX,
    };
    
    HandleNameType node_name;
    HandleIndexType node_index;

    glTFHandle(HandleNameType name, HandleIndexType index)
        : node_name(std::move(name))
        , node_index(index)
    {
        
    }

    glTFHandle()
        : node_name()
        , node_index(glTF_ELEMENT_INVALID_HANDLE)
    {
        
    }

    bool IsValid() const
    {
        return glTF_ELEMENT_INVALID_HANDLE != node_index || !node_name.empty();
    }

    bool operator<(const glTFHandle& other) const
    {
        return node_index < other.node_index ? true : (node_index > other.node_index ? false :
            node_name < other.node_name);
    }
};

//typedef unsigned glTFHandle;

struct glTF_Element_Base
{
    // Only enable default construction
    glTF_Element_Base() = default;

    // delete all copy and move function
    glTF_Element_Base(const glTF_Element_Base& rhs) = delete;
    glTF_Element_Base& operator=(const glTF_Element_Base& rhs) = delete;
    glTF_Element_Base(glTF_Element_Base&& rhs) = delete;
    glTF_Element_Base& operator=(glTF_Element_Base&& rhs) = delete;
    virtual ~glTF_Element_Base() = default;

    glTFHandle self_handle;
    std::string name;
};

template <glTF_Element_Type ElementType>
struct glTF_Element_Template : public glTF_Element_Base
{
    
};

// DECLARE glTF elements
#define DECLARE_GLTF_ELEMENT(Type) \
    template<>\
    struct glTF_Element_Template<Type>;

DECLARE_GLTF_ELEMENT(glTF_Element_Type::EScene)
DECLARE_GLTF_ELEMENT(glTF_Element_Type::ENode)
DECLARE_GLTF_ELEMENT(glTF_Element_Type::EMesh)
DECLARE_GLTF_ELEMENT(glTF_Element_Type::EAccessor)
DECLARE_GLTF_ELEMENT(glTF_Element_Type::EMaterial)
DECLARE_GLTF_ELEMENT(glTF_Element_Type::EBufferView)
DECLARE_GLTF_ELEMENT(glTF_Element_Type::EBuffer)
DECLARE_GLTF_ELEMENT(glTF_Element_Type::ETexture)
DECLARE_GLTF_ELEMENT(glTF_Element_Type::ESampler)
DECLARE_GLTF_ELEMENT(glTF_Element_Type::EImage)
DECLARE_GLTF_ELEMENT(glTF_Element_Type::ECamera)
DECLARE_GLTF_ELEMENT(glTF_Element_Type::ESkin)
DECLARE_GLTF_ELEMENT(glTF_Element_Type::EAnimation)
DECLARE_GLTF_ELEMENT(glTF_Element_Type::EAsset)

// Implement glTF elements

// ---------------------------------- Scene Type ---------------------------------- 
template<>
struct glTF_Element_Template<glTF_Element_Type::EScene> : glTF_Element_Base
{
    std::vector<glTFHandle> root_nodes;
};

typedef glTF_Element_Template<glTF_Element_Type::EScene> glTF_Element_Scene;

// ---------------------------------- Node Type ---------------------------------- 
struct glTF_Transform
{
    glTF_Transform(const glm::mat4& matrix = glm::mat4(1.0f))
        : m_matrix(matrix)
    {
        // TODO: @JACK implementation?
    }

    void SetMatrixData(const float* data);
    const glm::mat4& GetMatrix() const;
    
protected:
    mutable glm::mat4 m_matrix{};
};

template<>
struct glTF_Element_Template<glTF_Element_Type::ENode> : glTF_Element_Base
{
    glTFHandle parent{};
    glTFHandle camera{};
    //glTFHandle mesh{};
    glTF_Transform transform;
    std::vector<glTFHandle> meshes;
    std::vector<glTFHandle> children;

    // Root node cannot reference by children node array
    bool IsRoot() const
    {
        return parent.IsValid();
    }
};

typedef glTF_Element_Template<glTF_Element_Type::ENode>     glTF_Element_Node;

// ---------------------------------- Mesh Type ----------------------------------


struct glTF_Attribute_Base
{
    enum glTF_Attribute
    {
        EPosition   = 0,
        ENormal     = 1,
        ETangent    = 2,
        EColor      = 100,
        ETexCoord   = 200,
        EJoint      = 300,
        EWeight     = 400,
        EUnknown    = INT_MAX,
    };
    
    glTF_Attribute_Base() = delete;
};

std::string AttributeName(glTF_Attribute_Base::glTF_Attribute attribute);

typedef unsigned glTFAttributeId;

template<glTF_Attribute_Base::glTF_Attribute attribute, unsigned index = 0>
struct glTF_Attribute_Type : glTF_Attribute_Base
{
    const static glTFAttributeId attribute_type_id = attribute + index;
    glTF_Attribute_Type() = delete;
};

typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::EPosition>  glTF_Attribute_POSITION;
typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::ENormal>    glTF_Attribute_NORMAL;
typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::ETangent>   glTF_Attribute_TANGENT;

typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::EColor, 0>  glTF_Attribute_COLOR_0;
typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::EColor, 1>  glTF_Attribute_COLOR_1;
typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::EColor, 2>  glTF_Attribute_COLOR_2;
typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::EColor, 3>  glTF_Attribute_COLOR_3;

typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::ETexCoord, 0>  glTF_Attribute_TEXCOORD_0;
typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::ETexCoord, 1>  glTF_Attribute_TEXCOORD_1;
typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::ETexCoord, 2>  glTF_Attribute_TEXCOORD_2;
typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::ETexCoord, 3>  glTF_Attribute_TEXCOORD_3;

typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::EJoint, 0>  glTF_Attribute_Joint_0;
typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::EJoint, 1>  glTF_Attribute_Joint_1;
typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::EJoint, 2>  glTF_Attribute_Joint_2;
typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::EJoint, 3>  glTF_Attribute_Joint_3;

typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::EWeight, 0>  glTF_Attribute_Weight_0;
typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::EWeight, 1>  glTF_Attribute_Weight_1;
typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::EWeight, 2>  glTF_Attribute_Weight_2;
typedef glTF_Attribute_Type<glTF_Attribute_Base::glTF_Attribute::EWeight, 3>  glTF_Attribute_Weight_3;

struct glTF_Primitive
{
    enum glTF_Primitive_Mode
    {
        EPoints         = 0,
        ELineStrips     = 1,
        ELineLoops      = 2,
        ELines          = 3,
        ETriangles      = 4,
        ETriangleStrips = 5,
        ETriangleFans   = 6,
    };
    
    std::map<glTFAttributeId, glTFHandle> attributes;
    glTFHandle indices;
    glTFHandle material;
    glTF_Primitive_Mode mode;

    unsigned Hash() const
    {
        unsigned hash = 0;
        unsigned hash_index = 0;
        
        hash += attributes.size() * (hash_index++ * 213124);
        
        for (const auto& attribute : attributes)
        {
            hash += attribute.first * (hash_index++ * 12315);
            hash += attribute.second.node_index << (hash_index++ * 136514);
        }

        hash += indices.node_index * (hash_index++ * 923746);
        hash += material.node_index * (hash_index++ * 2143);

        hash += mode * (hash++ * 23423);
        return hash;
    }
};

template<>
struct glTF_Element_Template<glTF_Element_Type::EMesh> : glTF_Element_Base
{
    std::vector<glTF_Primitive> primitives;
};

typedef glTF_Element_Template<glTF_Element_Type::EMesh> glTF_Element_Mesh;


// ---------------------------------- Texture Type ----------------------------------
template<>
struct glTF_Element_Template<glTF_Element_Type::EImage> : glTF_Element_Base
{
    std::string uri;
    // TODO: support load image from buffer view
};

typedef glTF_Element_Template<glTF_Element_Type::EImage> glTF_Element_Image;

template<>
struct glTF_Element_Template<glTF_Element_Type::ESampler> : glTF_Element_Base
{
    enum glTF_Sampler_Filter
    {
        ENearest				= 9728,
        ELinear					= 9729,
        ENearestMipmapNearest	= 9984,
        ELinearMipmapNearest	= 9985,
        ENearestMipmapLinear	= 9986,
        ELinearMipmapLinear		= 9987,
    };

    enum glTF_Sampler_Wrapping
    {
        EClipToEdge 	= 33071,
        EMirroredRepeat = 33648,
        ERepeat 		= 10497,
        EDefault 		= 10497, 
    };
    
    glTF_Sampler_Filter mag_filter;
    glTF_Sampler_Filter min_filter;
    glTF_Sampler_Wrapping warp_s;
    glTF_Sampler_Wrapping warp_t;
};

typedef glTF_Element_Template<glTF_Element_Type::ESampler> glTF_Element_Sampler;

template<>
struct glTF_Element_Template<glTF_Element_Type::ETexture> : glTF_Element_Base
{
    glTFHandle sampler;
    glTFHandle source;
};

typedef glTF_Element_Template<glTF_Element_Type::ETexture> glTF_Element_Texture;

struct glTF_TextureInfo_Base
{
    glTFHandle index;
    unsigned texCoord_index {0};
};

struct glTF_TextureInfo_Normal : glTF_TextureInfo_Base
{
    // The scalar parameter applied to each normal vector of the texture. This value scales the normal vector in X and Y directions using the formula:
    // scaledNormal = normalize<sampled normal texture value> * 2.0 - 1.0) * vec3(<normal scale>, <normal scale>, 1.0).
    float scale {1.0f};
};

struct glTF_TextureInfo_Occlusion : glTF_TextureInfo_Base
{
    // A scalar parameter controlling the amount of occlusion applied. A value of 0.0 means no occlusion. A value of 1.0 means full occlusion.
    // This value affects the final occlusion value as: 1.0 + strength * (<sampled occlusion texture value> - 1.0).
    float strength {1.0f};
};

// ---------------------------------- Material Type ----------------------------------
struct glTF_Material_PBRMetallicRoughness
{
    glm::fvec4 base_color_factor {1.0f, 1.0f, 1.0f, 1.0f};
    glTF_TextureInfo_Base base_color_texture;
    
    float metallic_factor {1.0f};
    float roughness_factor {1.0f};
    
    glTF_TextureInfo_Base metallic_roughness_texture;
};

template<>
struct glTF_Element_Template<glTF_Element_Type::EMaterial> : glTF_Element_Base
{
    glTF_Material_PBRMetallicRoughness pbr;
    
    glTF_TextureInfo_Normal normal_texture;
    glTF_TextureInfo_Occlusion occlusion_texture;
    
    glTF_TextureInfo_Base emissive_texture;
    glm::fvec3 emissive_factor {0.0f, 0.0f, 0.0f};

    std::string alpha_mode {"OPAQUE"};
    float alpha_cutoff {0.5f};
    bool double_sided {false};
};

typedef glTF_Element_Template<glTF_Element_Type::EMaterial> glTF_Element_Material;

// TODO: @JACK Implement Morph Targets & Skin Mesh

// ---------------------------------- Buffer Type ----------------------------------
template<>
struct glTF_Element_Template<glTF_Element_Type::EBuffer> : glTF_Element_Base
{
    std::string uri;
    size_t byte_length;
};

typedef glTF_Element_Template<glTF_Element_Type::EBuffer> glTF_Element_Buffer;

// ---------------------------------- Buffer View Type ----------------------------------
template<>
struct glTF_Element_Template<glTF_Element_Type::EBufferView> : glTF_Element_Base
{
    enum glTF_BufferView_Target
    {
        EArrayBuffer        = 34962,
        EElementArrayBuffer = 34963,
    };

    glTFHandle buffer;
    size_t byte_offset;
    size_t byte_length;
    size_t byte_stride;
    glTF_BufferView_Target target;
};

typedef glTF_Element_Template<glTF_Element_Type::EBufferView> glTF_Element_BufferView;

// ---------------------------------- Accessor Type ----------------------------------
template<>
struct glTF_Element_Template<glTF_Element_Type::EAccessor> : glTF_Element_Base
{
    enum glTF_Accessor_Component_Type
    {
        EByte           = 5120,
        EUnsignedByte   = 5121,
        EShort          = 5122,
        EUnsignedShort  = 5123,
        // Official documentation lost 5124, should be EInt?
        EUnsignedInt    = 5125,
        EFloat          = 5126,
    };

    enum glTF_Accessor_Element_Type
    {
        EScalar,
        EVec2,
        EVec3,
        EVec4,
        EMat2,
        EMat3,
        EMat4,
        EUnknown,
    };
    
    glTFHandle buffer_view;
    size_t byte_offset;
    glTF_Accessor_Component_Type component_type;
    bool normalized;
    size_t count;
    glTF_Accessor_Element_Type element_type;
    // TODO: @JACK add sparse member

    unsigned GetComponentByteSize() const
    {
        switch (component_type) {
            case EByte: 
            case EUnsignedByte: return 1;
            case EShort:
            case EUnsignedShort: return 2;
            case EUnsignedInt: 
            case EFloat: return 4;
        }
        GLTF_CHECK(false);
        return 0;
    }
    
    unsigned GetElementByteSize() const
    {
        unsigned elementCount = 0;
        switch (element_type) {
            case glTF_Accessor_Element_Type::EScalar:   elementCount = 1; break;
            case glTF_Accessor_Element_Type::EVec2:     elementCount = 2; break;
            case glTF_Accessor_Element_Type::EVec3:     elementCount = 3; break;
            case glTF_Accessor_Element_Type::EVec4:     elementCount = 4; break;
            case glTF_Accessor_Element_Type::EMat2:     elementCount = 4; break;
            case glTF_Accessor_Element_Type::EMat3:     elementCount = 9; break;
            case glTF_Accessor_Element_Type::EMat4:     elementCount = 16; break;
            case glTF_Accessor_Element_Type::EUnknown: GLTF_CHECK(false); break;
        }
        return elementCount * GetComponentByteSize();
    }

    bool LoadData(std::unique_ptr<float[]>& outData, const glTF_Element_BufferView& bufferView) const
    {
        
    }
};

template<glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type component_type>
struct glTF_Element_Accessor_MinMax : glTF_Element_Template<glTF_Element_Type::EAccessor>
{
    
};

template <>
struct glTF_Element_Accessor_MinMax<glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EByte>
    : glTF_Element_Template<glTF_Element_Type::EAccessor>
{
    std::vector<int8_t> min;
    std::vector<int8_t> max;
};

template <>
struct glTF_Element_Accessor_MinMax<glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EUnsignedByte> : glTF_Element_Template<glTF_Element_Type::EAccessor>
{
    std::vector<uint8_t> min;
    std::vector<uint8_t> max;
};

template <>
struct glTF_Element_Accessor_MinMax<glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EShort> : glTF_Element_Template<glTF_Element_Type::EAccessor>
{
    std::vector<int16_t> min;
    std::vector<int16_t> max;
};

template <>
struct glTF_Element_Accessor_MinMax<glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EUnsignedShort> : glTF_Element_Template<glTF_Element_Type::EAccessor>
{
    std::vector<uint16_t> min;
    std::vector<uint16_t> max;
};

template <>
struct glTF_Element_Accessor_MinMax<glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EUnsignedInt> : glTF_Element_Template<glTF_Element_Type::EAccessor>
{
    std::vector<uint32_t> min;
    std::vector<uint32_t> max;
};

template <>
struct glTF_Element_Accessor_MinMax<glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EFloat> : glTF_Element_Template<glTF_Element_Type::EAccessor>
{
    std::vector<float> min;
    std::vector<float> max;
};

typedef glTF_Element_Template<glTF_Element_Type::EAccessor>                         glTF_Element_Accessor_Base;
typedef glTF_Element_Accessor_MinMax<glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EByte>           glTF_Element_Accessor_Byte;
typedef glTF_Element_Accessor_MinMax<glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EUnsignedByte>   glTF_Element_Accessor_UByte;
typedef glTF_Element_Accessor_MinMax<glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EShort>          glTF_Element_Accessor_Short;
typedef glTF_Element_Accessor_MinMax<glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EUnsignedShort>  glTF_Element_Accessor_UShort;
typedef glTF_Element_Accessor_MinMax<glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EUnsignedInt>    glTF_Element_Accessor_UInt;
typedef glTF_Element_Accessor_MinMax<glTF_Element_Template<glTF_Element_Type::EAccessor>::glTF_Accessor_Component_Type::EFloat>          glTF_Element_Accessor_Float;

// ---------------------------------- Asset Type ----------------------------------
struct glTF_Version
{
    unsigned major;
    unsigned minor;
};

template <>
struct glTF_Element_Template<glTF_Element_Type::EAsset>
{
    glTF_Version version;
    glTF_Version min_version;
};
