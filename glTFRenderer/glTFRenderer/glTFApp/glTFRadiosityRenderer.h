#pragma once

#include "glTFScene/glTFSceneTriangleMesh.h"
#include "glTFUtils/glTFUtils.h"
#include <embree3/rtcore.h>

struct MeshInstanceFormFactorData
{
    glTFUniqueID form_factor_id;
    unsigned world_vertices_indices[3];
    glm::vec3 world_normal;
    
    // other surface id - form factor
    std::map<glTFUniqueID, float> contributed_surfaces;

    // runtime solved irradiance
    glm::vec3 irradiance;

    // runtime emissive irradiance
    glm::vec3 emissive;
};

// TODO: overflow?
static glTFUniqueID EncodeFormFactorID(unsigned instance_id, unsigned face_id)
{
    const glTFUniqueID result = instance_id << 24 | face_id;
    return result;
}

static void DecodeFromFormFactorID(glTFUniqueID form_factor_id, unsigned& out_instance_id, unsigned& out_face_id)
{
    out_instance_id = form_factor_id >> 24;
    out_face_id = form_factor_id & 0x00ffffff;
}

class glTFRadiosityRenderer
{
public:
    glTFRadiosityRenderer();
    
    bool InitScene(const glTFSceneGraph& scene_graph);
    bool UpdateIndirectLighting(const glTFSceneGraph& scene_graph, bool recalculate);

    const std::map<glTFUniqueID, MeshInstanceFormFactorData>& GetRadiosityData() const;

protected:
    bool AddTriangle(const glTFSceneTriangleMesh& triangle_mesh);
    bool PrecomputeFormFactor();
    bool BuildAS();
    
    bool HasIntersectByEmbree(const glm::vec3& origin, const glm::vec3& dir, glTFUniqueID& out_id) const;
    bool HasIntersectByDebug(const glm::vec3& origin, const glm::vec3& dir) const;

    glm::vec3 GenerateRayDirWithinHemiSphere(const glm::vec3& world_normal, float& out_pdf);
    
    bool has_built {false};
    bool has_precomputed {false};
    bool include_direct_lighting {false};
    
    RTCDevice m_device {nullptr};
    RTCScene m_scene {nullptr};

    std::map<glTFUniqueID, MeshInstanceFormFactorData> m_form_factors;
    std::map<glTFUniqueID, std::vector<glm::vec3>> m_mesh_world_positions;
};
