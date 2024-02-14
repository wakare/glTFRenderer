#pragma once
#include "glTFUtils/glTFUtils.h"

class glTFVulkanTest
{
public:
    glTFVulkanTest() = default;
    ~glTFVulkanTest() = default;
    DECLARE_NON_COPYABLE(glTFVulkanTest)
    
    bool Init();
    bool Update();
};
