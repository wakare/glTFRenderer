#pragma once
#include <map>
#include <string>
#include "glm.hpp"

#define GLTF_CHECK(a) assert(a)

enum class glTF_Element_Type
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

typedef unsigned glTFHandle;
#define glTF_ELEMENT_INVALID_HANDLE UINT_MAX

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

    glTFHandle self_handle = glTF_ELEMENT_INVALID_HANDLE;
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
    glTF_Transform(
        const glm::vec3& translation,
        const glm::vec4& rotation,
        const glm::vec3& scale)
    {
        // TODO: @JACK implementation?
    }

    glTF_Transform()
    = default;

    glm::mat4x4 matrix{};
};

template<>
struct glTF_Element_Template<glTF_Element_Type::ENode> : glTF_Element_Base
{
    glTFHandle parent{};
    glTFHandle camera{};
    glTFHandle mesh{};
    glTF_Transform transform;
    std::vector<glTFHandle> children;

    // Root node cannot reference by children node array
    bool IsRoot() const
    {
        return parent == glTF_ELEMENT_INVALID_HANDLE;
    }
};

typedef glTF_Element_Template<glTF_Element_Type::ENode>     glTF_Element_Node;

// ---------------------------------- Mesh Type ----------------------------------
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

typedef unsigned glTFAttributeId;

struct glTF_Attribute_Base
{
    glTF_Attribute_Base() = delete;
};

template<glTF_Attribute attribute, unsigned index = 0>
struct glTF_Attribute_Type : glTF_Attribute_Base
{
    const static glTFAttributeId attribute_type_id = attribute + index;
    glTF_Attribute_Type() = delete;
};

typedef glTF_Attribute_Type<glTF_Attribute::EPosition>  glTF_Attribute_POSITION;
typedef glTF_Attribute_Type<glTF_Attribute::ENormal>    glTF_Attribute_NORMAL;
typedef glTF_Attribute_Type<glTF_Attribute::ETangent>   glTF_Attribute_TANGENT;

typedef glTF_Attribute_Type<glTF_Attribute::EColor, 0>  glTF_Attribute_COLOR_0;
typedef glTF_Attribute_Type<glTF_Attribute::EColor, 1>  glTF_Attribute_COLOR_1;
typedef glTF_Attribute_Type<glTF_Attribute::EColor, 2>  glTF_Attribute_COLOR_2;
typedef glTF_Attribute_Type<glTF_Attribute::EColor, 3>  glTF_Attribute_COLOR_3;

typedef glTF_Attribute_Type<glTF_Attribute::ETexCoord, 0>  glTF_Attribute_TexCoord_0;
typedef glTF_Attribute_Type<glTF_Attribute::ETexCoord, 1>  glTF_Attribute_TexCoord_1;
typedef glTF_Attribute_Type<glTF_Attribute::ETexCoord, 2>  glTF_Attribute_TexCoord_2;
typedef glTF_Attribute_Type<glTF_Attribute::ETexCoord, 3>  glTF_Attribute_TexCoord_3;

typedef glTF_Attribute_Type<glTF_Attribute::EJoint, 0>  glTF_Attribute_Joint_0;
typedef glTF_Attribute_Type<glTF_Attribute::EJoint, 1>  glTF_Attribute_Joint_1;
typedef glTF_Attribute_Type<glTF_Attribute::EJoint, 2>  glTF_Attribute_Joint_2;
typedef glTF_Attribute_Type<glTF_Attribute::EJoint, 3>  glTF_Attribute_Joint_3;

typedef glTF_Attribute_Type<glTF_Attribute::EWeight, 0>  glTF_Attribute_Weight_0;
typedef glTF_Attribute_Type<glTF_Attribute::EWeight, 1>  glTF_Attribute_Weight_1;
typedef glTF_Attribute_Type<glTF_Attribute::EWeight, 2>  glTF_Attribute_Weight_2;
typedef glTF_Attribute_Type<glTF_Attribute::EWeight, 3>  glTF_Attribute_Weight_3;

enum class glTF_Primitive_Mode
{
    EPoints         = 0,
    ELineStrips     = 1,
    ELineLoops      = 2,
    ELines          = 3,
    ETriangles      = 4,
    ETriangleStrips = 5,
    ETriangleFans   = 6,
};

struct glTF_Primitive
{
    std::map<glTFAttributeId, std::vector<glTFHandle>> attributes;
    glTFHandle indices;
    glTFHandle material;
    glTF_Primitive_Mode mode;
};

template<>
struct glTF_Element_Template<glTF_Element_Type::EMesh> : glTF_Element_Base
{
    std::vector<glTF_Primitive> primitives;
};

typedef glTF_Element_Template<glTF_Element_Type::EMesh> glTF_Element_Mesh;

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
enum glTF_BufferView_Target
{
    EArrayBuffer        = 34962,
    EElementArrayBuffer = 34963,
};

template<>
struct glTF_Element_Template<glTF_Element_Type::EBufferView> : glTF_Element_Base
{
    glTFHandle buffer;
    size_t byte_offset;
    size_t byte_length;
    size_t byte_stride;
    glTF_BufferView_Target target;
};

typedef glTF_Element_Template<glTF_Element_Type::EBufferView> glTF_Element_BufferView;

// ---------------------------------- Accessor Type ----------------------------------
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

enum class glTF_Accessor_Element_Type
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

template<>
struct glTF_Element_Template<glTF_Element_Type::EAccessor> : glTF_Element_Base
{
    glTFHandle buffer_view;
    size_t byte_offset;
    glTF_Accessor_Component_Type component_type;
    bool normalized;
    size_t count;
    glTF_Accessor_Element_Type element_type;
    // TODO: @JACK add sparse member
};

template<glTF_Accessor_Component_Type component_type>
struct glTF_Element_Accessor_MinMax : glTF_Element_Template<glTF_Element_Type::EAccessor>
{
    
};

template <>
struct glTF_Element_Accessor_MinMax<glTF_Accessor_Component_Type::EByte> : glTF_Element_Template<glTF_Element_Type::EAccessor>
{
    std::vector<int8_t> min;
    std::vector<int8_t> max;
};

template <>
struct glTF_Element_Accessor_MinMax<glTF_Accessor_Component_Type::EUnsignedByte> : glTF_Element_Template<glTF_Element_Type::EAccessor>
{
    std::vector<uint8_t> min;
    std::vector<uint8_t> max;
};

template <>
struct glTF_Element_Accessor_MinMax<glTF_Accessor_Component_Type::EShort> : glTF_Element_Template<glTF_Element_Type::EAccessor>
{
    std::vector<int16_t> min;
    std::vector<int16_t> max;
};

template <>
struct glTF_Element_Accessor_MinMax<glTF_Accessor_Component_Type::EUnsignedShort> : glTF_Element_Template<glTF_Element_Type::EAccessor>
{
    std::vector<uint16_t> min;
    std::vector<uint16_t> max;
};

template <>
struct glTF_Element_Accessor_MinMax<glTF_Accessor_Component_Type::EUnsignedInt> : glTF_Element_Template<glTF_Element_Type::EAccessor>
{
    std::vector<uint32_t> min;
    std::vector<uint32_t> max;
};

template <>
struct glTF_Element_Accessor_MinMax<glTF_Accessor_Component_Type::EFloat> : glTF_Element_Template<glTF_Element_Type::EAccessor>
{
    std::vector<float> min;
    std::vector<float> max;
};

typedef glTF_Element_Template<glTF_Element_Type::EAccessor>                         glTF_Element_Accessor_Base;
typedef glTF_Element_Accessor_MinMax<glTF_Accessor_Component_Type::EByte>           glTF_Element_Accessor_Byte;
typedef glTF_Element_Accessor_MinMax<glTF_Accessor_Component_Type::EUnsignedByte>   glTF_Element_Accessor_UByte;
typedef glTF_Element_Accessor_MinMax<glTF_Accessor_Component_Type::EShort>          glTF_Element_Accessor_Short;
typedef glTF_Element_Accessor_MinMax<glTF_Accessor_Component_Type::EUnsignedShort>  glTF_Element_Accessor_UShort;
typedef glTF_Element_Accessor_MinMax<glTF_Accessor_Component_Type::EUnsignedInt>    glTF_Element_Accessor_UInt;
typedef glTF_Element_Accessor_MinMax<glTF_Accessor_Component_Type::EFloat>          glTF_Element_Accessor_Float;

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
