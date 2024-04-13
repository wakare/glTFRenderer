#include "glTFRadiosityRenderer.h"
#define NOMINMAX
#include <Windows.h>

#include "glTFLight/glTFDirectionalLight.h"
#include "glTFLight/glTFLightBase.h"
#include "glTFScene/glTFSceneGraph.h"

float luminance(const glm::vec3& rgb)
{
    return dot(rgb, {0.2126f, 0.7152f, 0.0722f});
}

// Calculates rotation quaternion from input vector to the vector (0, 0, 1)
// Input vector must be normalized!
glm::vec4 getRotationToZAxis(glm::vec3 input) {

    // Handle special case when input is exact or near opposite of (0, 0, 1)
    if (input.z < -0.99999f) return glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

    return normalize(glm::vec4(input.y, -input.x, 0.0f, 1.0f + input.z));
}

// Calculates rotation quaternion from vector (0, 0, 1) to the input vector
// Input vector must be normalized!
glm::vec4 getRotationFromZAxis(glm::vec3 input) {

    // Handle special case when input is exact or near opposite of (0, 0, 1)
    if (input.z < -0.99999f) return glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

    return normalize(glm::vec4(-input.y, input.x, 0.0f, 1.0f + input.z));
}

// Returns the quaternion with inverted rotation
glm::vec4 invertRotation(glm::vec4 q)
{
    return glm::vec4(-q.x, -q.y, -q.z, q.w);
}

// Optimized point rotation using quaternion
// Source: https://gamedev.stackexchange.com/questions/28395/rotating-vector3-by-a-quaternion
glm::vec3 rotatePoint(glm::vec4 q, glm::vec3 v) {
    const glm::vec3 qAxis = glm::vec3(q.x, q.y, q.z);
    return 2.0f * dot(qAxis, v) * qAxis + (q.w * q.w - dot(qAxis, qAxis)) * v + 2.0f * q.w * cross(qAxis, v);
}


// Samples a direction within a hemisphere oriented along +Z axis with a cosine-weighted distribution 
// Source: "Sampling Transformations Zoo" in Ray Tracing Gems by Shirley et al.
#define PI 3.1415926f
#define TWO_PI (2.0f * PI)
#define ONE_OVER_PI (1.0f / PI)

glm::vec3 sampleHemisphereUniform(glm::vec2 u, float& pdf)
{
    float a = glm::acos(u.x);
    float b = TWO_PI * u.y;

    const glm::vec3 result =
        {
            sin(a) * cos(b),
            sin(a) * sin(b),
            cos(a)
        };

    pdf = 0.5f * ONE_OVER_PI;
    return result;
}

glm::vec3 sampleHemisphereCosine(glm::vec2 u, float& pdf)
{
    double sqrt_rand = sqrt(static_cast<double>(u.x));
    GLTF_CHECK(sqrt_rand >= 0.0f && sqrt_rand <= 1.0f);
    
    float a = static_cast<float>(acos(sqrt_rand));
    float b = TWO_PI * u.y;

    const glm::vec3 result = glm::vec3(
        sin(a) * cos(b),
        sin(a) * sin(b),
        sqrt_rand
    );

    pdf = result.z * ONE_OVER_PI;
    
    return result;
}

// must normalize direction of ray
struct Tri
{
    glm::vec3 v1;
    glm::vec3 v2;
    glm::vec3 v3;
};

bool IntersectRayTri(Tri& tri, glm::vec3 o, glm::vec3 n) {
    glm::vec3 e1, e2, pvec, qvec, tvec;

    e1 = tri.v2 - tri.v1;
    e2 = tri.v3 - tri.v1;
    pvec = glm::cross(n, e2);
    
    n = glm::normalize(n);
    //NORMALIZE(pvec);
    float det = glm::dot(pvec, e1);

    if (det != 0)
    {
        float invDet = 1.0f / det;
        tvec = o - tri.v1;
        // NORMALIZE(tvec);
        float u = invDet * glm::dot(tvec, pvec);
        if (u < 0.0f || u > 1.0f)
        {

            return false;
        }
        qvec = glm::cross(tvec, e1);
        // NORMALIZE(qvec);
        float v = invDet * glm::dot(qvec, n);
        if (v < 0.0f || u + v > 1.0f)
        {

            return false;
        }
    }
    else return false;
    return true; // det != 0 and all tests for false intersection fail
}

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
    std::vector<glm::vec3> mesh_positions;
    
    RTCGeometry mesh = rtcNewGeometry(m_device, RTC_GEOMETRY_TYPE_TRIANGLE);

    const size_t vertex_count = triangle_mesh.GetMeshRawData().GetPositionOnlyBuffer().vertex_count;
    const size_t index_count = triangle_mesh.GetMeshRawData().GetIndexBuffer().index_count;
    const size_t face_count = index_count / 3;
    
    // Cache world space triangle infos
    const auto& triangle_mesh_vertices = triangle_mesh.GetPositionOnlyBufferData();
    const auto& triangle_mesh_indices = triangle_mesh.GetIndexBufferData();
    
    mesh_positions.resize(vertex_count);
    size_t stride = 0;
    
    for (size_t i = 0; i < vertex_count; ++i)
    {
        triangle_mesh_vertices.GetVertexAttributeDataByIndex(VertexAttributeType::POSITION, i, &mesh_positions[i].x, stride);
        mesh_positions[i] = glm::vec3(triangle_mesh.GetTransformMatrix() * glm::vec4(mesh_positions[i], 1.0f));
    }
    
    void* vertex_buffer = rtcSetNewGeometryBuffer(mesh, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * sizeof(float), vertex_count);
    GLTF_CHECK (triangle_mesh.GetMeshRawData().GetPositionOnlyBuffer().byteSize == (vertex_count * 3 * sizeof(float)));

    memcpy(vertex_buffer, mesh_positions.data(), (vertex_count * 3 * sizeof(float)));

    // embree do not support r16 index buffer format, convert to r32 index buffer
    std::vector<unsigned> index_buffer_temp(index_count);
    for (size_t i = 0; i < index_count; ++i)
    {
        index_buffer_temp[i] = triangle_mesh.GetIndexBufferData().GetIndexByOffset(i);
    }
     
    void* index_buffer = rtcSetNewGeometryBuffer(mesh, RTC_BUFFER_TYPE_INDEX, 0,
        RTC_FORMAT_UINT3, sizeof(UINT) * 3, face_count );

    memcpy(index_buffer, index_buffer_temp.data(), index_buffer_temp.size() * sizeof(unsigned));

    rtcCommitGeometry(mesh);
    const unsigned instance_id = rtcAttachGeometry(m_scene, mesh);
    rtcReleaseGeometry(mesh);

    for (size_t i = 0; i < face_count; ++i)
    {
        unsigned indices[] = {
            triangle_mesh_indices.GetIndexByOffset(3 * i),
            triangle_mesh_indices.GetIndexByOffset(3 * i + 1),
            triangle_mesh_indices.GetIndexByOffset(3 * i + 2),
        };
        
        glTFUniqueID form_factor_id = EncodeFormFactorID(instance_id, i);
        GLTF_CHECK(m_form_factors.find(form_factor_id) == m_form_factors.end());

        const auto& edge0 = glm::normalize(mesh_positions[indices[0]] - mesh_positions[indices[1]]);
        const auto& edge1 = glm::normalize(mesh_positions[indices[1]] - mesh_positions[indices[2]]);
        GLTF_CHECK(glm::abs(glm::dot(edge0, edge1)) < 0.99999f);
        
        const glm::vec3 face_world_normal = glm::normalize(glm::cross(edge0, edge1));

        MeshInstanceFormFactorData data{};
        data.form_factor_id = form_factor_id;
        data.world_vertices_indices[0] = indices[0];
        data.world_vertices_indices[1] = indices[1];
        data.world_vertices_indices[2] = indices[2];
        data.world_normal = face_world_normal;
        
        m_form_factors[form_factor_id] = std::move(data);
    }

    m_mesh_world_positions[instance_id] = std::move(mesh_positions);
    
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

    constexpr unsigned form_factor_ray_count = 4096;
    
    // Traverse all faces to calculate form factor
    for ( auto& form_factor : m_form_factors)
    {
        unsigned instance_id, primitive_id;
        DecodeFromFormFactorID(form_factor.first, instance_id, primitive_id );
        auto it_instance_world_vertices = m_mesh_world_positions.find(instance_id);
        if (it_instance_world_vertices == m_mesh_world_positions.end())
        {
            GLTF_CHECK(false);
            continue;
        }

        const auto& vertices = it_instance_world_vertices->second;

        std::map<glTFUniqueID, float> form_factor_contribute_results;
        const glm::vec3 world_vertices[3] =
        {
            vertices[form_factor.second.world_vertices_indices[0]],
            vertices[form_factor.second.world_vertices_indices[1]],
            vertices[form_factor.second.world_vertices_indices[2]],
        };

        float total_weight = 0.0f;
        unsigned total_ray_count = 0;
        for (unsigned i = 0; i < form_factor_ray_count; ++i)
        {
            float sample_pdf;
            glm::vec3 ray_dir = GenerateRayDirWithinHemiSphere(form_factor.second.world_normal, sample_pdf);
            if (sample_pdf <= 0.0f)
            {
                continue;
            }
            
            const float weight = 1.0f / sample_pdf;
            ++total_ray_count;
            total_weight += weight;
            const glm::vec3 rand_barycentric_coord = _generate_rand_barycentric_coord();
            const glm::vec3 test_point =
                    world_vertices[0] * rand_barycentric_coord.x +
                    world_vertices[1] * rand_barycentric_coord.y +
                    world_vertices[2] * rand_barycentric_coord.z;
            
            glTFUniqueID out_id;
            if (HasIntersectByEmbree(test_point, ray_dir, out_id))
            {
                const float falloff = glm::max(glm::dot(m_form_factors[out_id].world_normal, -ray_dir) * glm::dot(form_factor.second.world_normal, ray_dir), 0.0f);  
                form_factor_contribute_results[out_id] += (falloff * weight);
            }
        }

        LOG_FORMAT_FLUSH("[DEBUG] Total weight / raycount is %f\n", total_weight / total_ray_count);

        for (const auto& contribute_form_factor_info : form_factor_contribute_results)
        {
            auto& contribute_form_factor = m_form_factors[contribute_form_factor_info.first];
            contribute_form_factor.contributed_surfaces[form_factor.first] = contribute_form_factor_info.second / (total_ray_count * TWO_PI);
            form_factor.second.contributed_surfaces[contribute_form_factor_info.first] = contribute_form_factor.contributed_surfaces[form_factor.first];
        }
    }

    return true;
}

bool glTFRadiosityRenderer::UpdateIndirectLighting(const glTFSceneGraph& scene_graph, bool recalculate)
{
    unsigned form_factor_transport_count = 5;
    
    if (recalculate)
    {
        form_factor_transport_count = 10;
        for (auto& form_factor : m_form_factors)
        {
            form_factor.second.emissive = {0.0f, 0.0f, 0.0f};
            form_factor.second.irradiance = {0.0f, 0.0f, 0.0f};
        }
        
        // inject direct lighting
        scene_graph.TraverseNodes([this](const glTFSceneNode& node)
        {
            for (const auto& object : node.m_objects)
            {
                if (const glTFLightBase* light_base = dynamic_cast<const glTFLightBase*>(object.get()))
                {
                    switch (light_base->GetType()) {
                    case glTFLightType::PointLight:
                        
                        break;
                    case glTFLightType::DirectionalLight:
                        if (const glTFDirectionalLight* directional_light = dynamic_cast<const glTFDirectionalLight*>(light_base))
                        {
                            for (auto& form_factor : m_form_factors)
                        {
                            const float cosine = glm::dot(form_factor.second.world_normal, -directional_light->GetDirection());
                            if (cosine < 0.0f)
                            {
                                continue;
                            }
                                
                            // visibility test
                            const auto& indices = form_factor.second.world_vertices_indices;

                            unsigned instance_id, primitive_id;
                            DecodeFromFormFactorID(form_factor.first, instance_id, primitive_id);
                            const auto& mesh_vertices = m_mesh_world_positions[instance_id];
                            glm::vec3 vertices[3] =
                            {
                                mesh_vertices[indices[0]],
                                mesh_vertices[indices[1]],
                                mesh_vertices[indices[2]],
                            };

                            constexpr unsigned test_count = 4;
                            glm::vec3 test_vertices[test_count] =
                            {
                                vertices[0] * 0.5f + vertices[1] * 0.25f + vertices[2] * 0.25f,
                                vertices[0] * 0.25f + vertices[1] * 0.5f + vertices[2] * 0.25f,
                                vertices[0] * 0.25f + vertices[1] * 0.25f + vertices[2] * 0.5f,
                                vertices[0] * 0.33f + vertices[1] * 0.33f + vertices[2] * 0.33f,
                            };

                            float visibility_points = 0.0f;
                            for (unsigned i = 0; i < test_count; ++i)
                            {
                                const auto& test_vertex = test_vertices[i];
                                glm::vec3 dir = -directional_light->GetDirection();
                                
                                glTFUniqueID out_id;
                                if (HasIntersectByEmbree(test_vertex, dir, out_id))
                                {
                                    visibility_points += 1.0f;
                                }
                            }

                            visibility_points *= (1.0f / test_count);
                            if (visibility_points > 0.0f)
                            {
                                form_factor.second.emissive += visibility_points * directional_light->GetColor() * directional_light->GetIntensity() * cosine; 
                            }
                        }
                        }
                        
                        break;
                    case glTFLightType::SpotLight:
                        
                        break;
                    case glTFLightType::SkyLight:
                        
                        break;
                    }
                }
            }
            return true;
        });
    }
    
    for (unsigned i = 0; i < form_factor_transport_count; ++i)
    {
        float delta_emissive = 0.0f;
        
        for ( auto& form_factor : m_form_factors)
        {
            for (const auto& contribute_surface : form_factor.second.contributed_surfaces)
            {
                const glm::vec3 transport_emissive = contribute_surface.second * form_factor.second.emissive;
                
                auto& contribute_form_factor = m_form_factors[contribute_surface.first];
                contribute_form_factor.irradiance += transport_emissive;
                contribute_form_factor.emissive += transport_emissive;

                delta_emissive += luminance(transport_emissive);
            }
            
            form_factor.second.emissive = {0.0f, 0.0f, 0.0f};
        }

        // No more emissive transport, exit calculation
        if (delta_emissive < 1e-3f)
        {
            break;
        }
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

bool glTFRadiosityRenderer::HasIntersectByEmbree(const glm::vec3& origin, const glm::vec3& dir, glTFUniqueID& out_id) const
{
    RTCRayHit rayhit {};
    rayhit.ray.org_x = origin.x;
    rayhit.ray.org_y = origin.y;
    rayhit.ray.org_z = origin.z;
    rayhit.ray.dir_x = dir.x;
    rayhit.ray.dir_y = dir.y;
    rayhit.ray.dir_z = dir.z;
    rayhit.ray.tnear = 0.0001f;
    rayhit.ray.tfar = std::numeric_limits<float>::max();
    rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

    RTCIntersectContext context;
    rtcInitIntersectContext(&context);

    rtcIntersect1(m_scene, &context, &rayhit);
    out_id = rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID ? EncodeFormFactorID(rayhit.hit.geomID, rayhit.hit.primID) : glTFUniqueIDInvalid; 
 
    return rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID;
}

bool glTFRadiosityRenderer::HasIntersectByDebug(const glm::vec3& origin, const glm::vec3& dir) const
{
    for (const auto& form_factor : m_form_factors)
    {
        unsigned instance_id, primitive_id;
        DecodeFromFormFactorID(form_factor.first, instance_id, primitive_id);

        auto it_mesh = m_mesh_world_positions.find(instance_id);
        const auto& mesh_position = it_mesh->second;
        Tri tri;
        tri.v1 = mesh_position[form_factor.second.world_vertices_indices[0]];
        tri.v2 = mesh_position[form_factor.second.world_vertices_indices[1]];
        tri.v3 = mesh_position[form_factor.second.world_vertices_indices[2]];

        if (IntersectRayTri(tri, origin, dir))
        {
            return true;
        }
    }
    
    return false;
}

glm::vec3 glTFRadiosityRenderer::GenerateRayDirWithinHemiSphere(const glm::vec3& world_normal, float& out_pdf)
{
    const glm::vec3 local_rotation = sampleHemisphereCosine(glm::vec2(Rand01(), Rand01()), out_pdf);
    //const glm::vec3 local_rotation = sampleHemisphereUniform(glm::vec2(Rand01(), Rand01()), out_pdf);
    const glm::vec3 ray_dir = glm::normalize(rotatePoint(invertRotation(getRotationToZAxis(world_normal)), local_rotation));
    return ray_dir;
}
