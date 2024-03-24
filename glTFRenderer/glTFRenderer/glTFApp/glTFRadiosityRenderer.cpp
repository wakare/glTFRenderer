#include "glTFRadiosityRenderer.h"
#include <Windows.h>

#include "glTFScene/glTFSceneGraph.h"

glTFRadiosityRenderer::glTFRadiosityRenderer()
{
    m_device = rtcNewDevice(NULL);
    m_scene = rtcNewScene(m_device);
}

bool glTFRadiosityRenderer::InitScene(const glTFSceneGraph& scene_graph)
{
    scene_graph.TraverseNodes([this](const glTFSceneNode& node)
    {
        for (auto& scene_object : node.m_objects)
        {
            if (const glTFSceneTriangleMesh* triangle = dynamic_cast<const glTFSceneTriangleMesh*>(scene_object.get()))
            {
                AddTriangle(*triangle);
            }
        }
        
        return true;
    });

    PrecomputeFormFactor();
    return true;
}

bool glTFRadiosityRenderer::AddTriangle(const glTFSceneTriangleMesh& triangle_mesh)
{
    RTCGeometry mesh = rtcNewGeometry(m_device, RTC_GEOMETRY_TYPE_TRIANGLE);

    const size_t vertex_count = triangle_mesh.GetMeshRawData().GetPositionOnlyBuffer().vertex_count;
    void* vertex_buffer = rtcSetNewGeometryBuffer(mesh, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * sizeof(float), vertex_count);
    GLTF_CHECK (triangle_mesh.GetMeshRawData().GetPositionOnlyBuffer().byteSize == (vertex_count * 3 * sizeof(float)));
    memcpy(vertex_buffer, triangle_mesh.GetPositionOnlyBufferData().data.get(), (vertex_count * 3 * sizeof(float)));

    const size_t index_count = triangle_mesh.GetMeshRawData().GetIndexBuffer().index_count;
    const size_t face_count = index_count / 3;

    // embree do not support r16 index buffer format, convert to r32 index buffer
    std::vector<unsigned> index_buffer_temp(index_count);
    for (size_t i = 0; i < index_count; ++i)
    {
        index_buffer_temp[i] = triangle_mesh.GetIndexBufferData().GetIndexByOffset(i);
    }
     
    void* index_buffer = rtcSetNewGeometryBuffer(mesh, RTC_BUFFER_TYPE_INDEX, 0,
        RTC_FORMAT_UINT3, sizeof(UINT) * 3, face_count );

    memcpy(index_buffer, index_buffer_temp.data(), index_buffer_temp.size() * sizeof(unsigned));

    rtcSetGeometryTransform(mesh, 0, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, &triangle_mesh.GetTransformMatrix()[0]);
    
    rtcCommitGeometry(mesh);
    rtcAttachGeometry(m_scene, mesh);
    rtcReleaseGeometry(mesh);

    // Cache world space triangle infos
    const auto& triangle_mesh_vertices = triangle_mesh.GetPositionOnlyBufferData();
    const auto& triangle_mesh_indices = triangle_mesh.GetIndexBufferData();
    for (size_t i = 0; i < face_count; ++i)
    {
        unsigned indices[] = {
            triangle_mesh_indices.GetIndexByOffset(3 * i),
            triangle_mesh_indices.GetIndexByOffset(3 * i + 1),
            triangle_mesh_indices.GetIndexByOffset(3 * i + 2),
        };

        std::vector<glm::vec3> position(3);
        size_t position_attribute_stride = 0;
        for (unsigned j = 0; j < 3; ++j)
        {
            triangle_mesh_vertices.GetVertexAttributeDataByIndex(VertexAttributeType::POSITION, indices[j], &position[j].x, position_attribute_stride);
            // Transform to world space
            position[j] = glm::vec3(triangle_mesh.GetTransformMatrix() * glm::vec4(position[j], 1.0f));
        }
        GLTF_CHECK(position_attribute_stride == 3 * sizeof(float));

        glTFUniqueID form_factor_id = MakeFormFactorID(triangle_mesh.GetID(), i);
        GLTF_CHECK(m_form_factors.find(form_factor_id) == m_form_factors.end());
        m_form_factors[form_factor_id] =
        {
            form_factor_id,
            position,
        };
    }
    
    return true;
}

bool glTFRadiosityRenderer::PrecomputeFormFactor()
{
    if (m_form_factors.empty())
    {
        GLTF_CHECK(false);
        return false;
    }
    
    BuildAS();

    static auto _generate_rand_barycentric_coord = []()
    {
        const float rand0 = Rand01();
        const float rand1 = Rand01() * (1 - rand0);
        GLTF_CHECK(rand0 >= 0.0f && rand1 >= 0.0f && (1.0f - rand0 - rand1) >= 0.0f);
        return glm::vec3(rand0, rand1, 1.0f - rand0 -rand1);
    };

    // Traverse all faces to calculate form factor
    for ( auto& form_factor : m_form_factors)
    {
        for ( auto& other_form_factor : m_form_factors)
        {
            if (form_factor.first == other_form_factor.first)
            {
                continue;
            }

            // Get random point and test intersect result
            static unsigned form_factor_loop_count = 1;
            float total_visibility = 0.0f;
            for (unsigned i = 0; i < form_factor_loop_count; ++i)
            {
                //const glm::vec3 rand_barycentric_coord0 = _generate_rand_barycentric_coord();
                //const glm::vec3 rand_barycentric_coord1 = _generate_rand_barycentric_coord();
                const glm::vec3 rand_barycentric_coord0 = {0.333f, 0.333f, 0.333f};
                const glm::vec3 rand_barycentric_coord1 = {0.333f, 0.333f, 0.333f};

                const glm::vec3 test_point0 =
                    form_factor.second.world_vertices[0] * rand_barycentric_coord0.x +
                    form_factor.second.world_vertices[1] * rand_barycentric_coord0.y +
                    form_factor.second.world_vertices[2] * rand_barycentric_coord0.z;

                const glm::vec3 test_point0_edge0 = form_factor.second.world_vertices[1] - form_factor.second.world_vertices[0];
                const glm::vec3 test_point0_edge1 = form_factor.second.world_vertices[2] - form_factor.second.world_vertices[1];
                const glm::vec3 test_point0_normal = normalize(glm::cross(test_point0_edge0, test_point0_edge1));
                
                const glm::vec3 test_point1 =
                    other_form_factor.second.world_vertices[0] * rand_barycentric_coord1.x +
                    other_form_factor.second.world_vertices[1] * rand_barycentric_coord1.y +
                    other_form_factor.second.world_vertices[2] * rand_barycentric_coord1.z;
                
                const glm::vec3 test_point1_edge0 = other_form_factor.second.world_vertices[1] - other_form_factor.second.world_vertices[0];
                const glm::vec3 test_point1_edge1 = other_form_factor.second.world_vertices[2] - other_form_factor.second.world_vertices[1];
                const glm::vec3 test_point1_normal = normalize(glm::cross(test_point1_edge0, test_point1_edge1));
                
                const glm::vec3 dir = normalize((test_point1 - test_point0));
                const float length = (test_point1 - test_point0).length();
                
                RTCRayHit rayhit;
                rayhit.ray.org_x = test_point0.x;
                rayhit.ray.org_y = test_point0.y;
                rayhit.ray.org_z = test_point0.z;
                rayhit.ray.dir_x = dir.x;
                rayhit.ray.dir_y = dir.y;
                rayhit.ray.dir_z = dir.z;
                rayhit.ray.tnear = 0.0001f;
                rayhit.ray.tfar = length - rayhit.ray.tnear;
                rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

                RTCIntersectContext context;
                rtcInitIntersectContext(&context);

                rtcIntersect1(m_scene, &context, &rayhit);
                if (rayhit.hit.geomID == RTC_INVALID_GEOMETRY_ID)
                {
                    // No occluded, add visibility
                    LOG_FORMAT_FLUSH("[Radiosity] (%f %f %f) -> (%f %f %f) is not occluded\n",
                        test_point0.x, test_point0.y, test_point0.z,
                        test_point1.x, test_point1.y, test_point1.z);
                    //total_visibility += glm::abs(glm::dot(test_point0_normal, dir) * glm::dot(test_point1_normal, dir));
                    total_visibility += 1.0f;
                }
            }
            
            total_visibility /= form_factor_loop_count;

            if (total_visibility > 0.0f)
            {
                form_factor.second.contributed_surfaces[other_form_factor.first] = total_visibility;
                other_form_factor.second.contributed_surfaces[form_factor.first] = total_visibility;
            }
        }
    }

    return true;
}

bool glTFRadiosityRenderer::UpdateIndirectLighting()
{
    for ( auto& form_factor : m_form_factors)
    {
        float form_factor_debug = 0.0f;
        for(const auto& surface : form_factor.second.contributed_surfaces)
        {
            form_factor_debug += surface.second;
        }

        form_factor.second.irradiance = glm::vec3(form_factor_debug,form_factor_debug,form_factor_debug);
    }
    
    return true;
}

const std::map<glTFUniqueID, MeshInstanceFormFactorData>& glTFRadiosityRenderer::GetRadiosityData() const
{
    return m_form_factors;
}

bool glTFRadiosityRenderer::BuildAS()
{
    if (has_built)
    {
        return true;
    }
    
    rtcCommitScene(m_scene);
    has_built = true;
    return true;
}
