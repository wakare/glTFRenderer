#include "glTFRenderInterfaceLighting.h"

#include "glTFLight/glTFDirectionalLight.h"
#include "glTFLight/glTFPointLight.h"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

glTFRenderInterfaceLighting::glTFRenderInterfaceLighting()
{
     AddInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<ConstantBufferPerLightDraw>>());
     AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<LightInfo>>());
}

bool glTFRenderInterfaceLighting::UpdateCPUBuffer()
{
     RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<ConstantBufferPerLightDraw>>()->UploadCPUBuffer(
          &m_light_buffer_data.light_info, sizeof(m_light_buffer_data.light_info)))
     
     if (!m_light_buffer_data.light_infos.empty())
     {
          RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceStructuredBuffer<LightInfo>>()->UploadCPUBuffer(
               m_light_buffer_data.light_infos.data(), m_light_buffer_data.light_infos.size() * sizeof(LightInfo)))
     }

     return true;
}

bool glTFRenderInterfaceLighting::UpdateLightInfo(const glTFLightBase& light)
{
     switch (light.GetType())
     {
     case glTFLightType::PointLight:
          {
               const glTFPointLight* point_light = dynamic_cast<const glTFPointLight*>(&light);
               LightInfo point_light_info{};
               point_light_info.position = glTF_Transform_WithTRS::GetTranslationFromMatrix(point_light->GetTransformMatrix());
               point_light_info.radius = point_light->GetRadius();
               point_light_info.intensity = {point_light->GetIntensity(), point_light->GetIntensity(), point_light->GetIntensity()};
               point_light_info.type = static_cast<unsigned>(glTFLightType::PointLight);

               if (m_light_buffer_data.light_indices.find(point_light->GetID()) == m_light_buffer_data.light_indices.end())
               {
                    m_light_buffer_data.light_indices[point_light->GetID()] = m_light_buffer_data.light_infos.size();
                    m_light_buffer_data.light_infos.push_back(point_light_info);
               }
               else
               {
                    m_light_buffer_data.light_infos[m_light_buffer_data.light_indices[point_light->GetID()]] = point_light_info;     
               }
          }
          break;
        
     case glTFLightType::DirectionalLight:
          {
               const glTFDirectionalLight* directional_light = dynamic_cast<const glTFDirectionalLight*>(&light);
               LightInfo directional_light_info{};
               directional_light_info.position = directional_light->GetDirection();
               directional_light_info.radius = 0.0f;
               directional_light_info.intensity = {directional_light->GetIntensity(), directional_light->GetIntensity(), directional_light->GetIntensity()};
               directional_light_info.type = static_cast<unsigned>(glTFLightType::DirectionalLight);
            
               if (m_light_buffer_data.light_indices.find(directional_light->GetID()) == m_light_buffer_data.light_indices.end())
               {
                    m_light_buffer_data.light_indices[directional_light->GetID()] = m_light_buffer_data.light_infos.size();
                    m_light_buffer_data.light_infos.push_back(directional_light_info);
               }
               else
               {
                    m_light_buffer_data.light_infos[m_light_buffer_data.light_indices[directional_light->GetID()]] = directional_light_info;     
               }
          }
          break;
     case glTFLightType::SpotLight: break;
     case glTFLightType::SkyLight: break;
     default: ;
     }

     m_light_buffer_data.light_info.light_count = m_light_buffer_data.light_infos.size();
     
     return true;
}
