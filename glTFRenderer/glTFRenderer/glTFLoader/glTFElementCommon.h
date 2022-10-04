#pragma once
#include "glm.hpp"

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
    EAnimation
};

typedef unsigned glTFElementHandle;

struct glTF_Element_Base
{
    glTFElementHandle self_handle;
};

template <glTF_Element_Type ElementType>
struct glTF_Element_Template : glTF_Element_Base
{
    
};

// DECLARE glTF elements
#define DECLARE_GLTF_ELEMENT(Type) \
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

// Implement glTF elements

// ---------------------------------- Scene Type ---------------------------------- 
template<>
struct glTF_Element_Template<glTF_Element_Type::EScene>
{
    std::vector<glTFElementHandle> nodes;
};

typedef glTF_Element_Template<glTF_Element_Type::EScene> glTF_Scene_Element;

// ---------------------------------- Node Type ---------------------------------- 
struct glTF_Transform
{
    glTF_Transform(
        const glm::vec3& translation,
        const glm::vec3& rotation,
        const glm::vec3& scale)
    {
        // TODO: @JACK implementation?
    }

    glTF_Transform()
    = default;

    glm::mat4x4 matrix{};
};

enum class glTF_Node_Type
{
    ENone,
    ECamera,
    EMesh,
};

template<>
struct glTF_Element_Template<glTF_Element_Type::ENode>
{
    glTF_Node_Type node_type;
    std::vector<glTFElementHandle> children;
};

template<glTF_Node_Type NodeType>
struct glTF_Element_Node_Template<NodeType> : glTF_Element_Template<glTF_Element_Type::ENode>
{
};

template<>
struct glTF_Element_Node_Template<glTF_Node_Type::ENone>
{
};

template<>
struct glTF_Element_Node_Template<glTF_Node_Type::ECamera>
{
    glTFElementHandle camera;
    glTF_Transform transform;
};

template<>
struct glTF_Element_Node_Template<glTF_Node_Type::EMesh>
{
    glTFElementHandle mesh;
    glTF_Transform transform;
};

typedef glTF_Element_Node_Template<glTF_Node_Type::ENone> glTF_Node_Element_None;
typedef glTF_Element_Node_Template<glTF_Node_Type::ECamera> glTF_Node_Element_Camera;
typedef glTF_Element_Node_Template<glTF_Node_Type::EMesh> glTF_Node_Element_Mesh;

// ---------------------------------- Mesh Type ----------------------------------
template<>
struct glTF_Element_Template<glTF_Element_Type::EMesh>
{
    
};

