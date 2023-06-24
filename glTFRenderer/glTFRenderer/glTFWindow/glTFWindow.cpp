#include "glTFWindow.h"
#include "../glTFLight/glTFDirectionalLight.h"
#include "../glTFLight/glTFPointLight.h"
#include "../glTFMaterial/glTFMaterialOpaque.h"
#include "../glTFRenderPass/glTFRenderPassLighting.h"
#include "../glTFRenderPass/glTFRenderPassMeshOpaque.h"
#include "../glTFScene/glTFSceneBox.h"
#include "../glTFScene/glTFCamera.h"
#include "../glTFLoader/glTFLoader.h"
#include "../glTFScene/glTFSceneTriangleMesh.h"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3native.h>

glTFWindow::glTFWindow()
    : m_glfwWindow(nullptr)
    , m_width(1920)
    , m_height(1080)
{
    
}

glTFTimer::glTFTimer()
    : m_deltaTick(0)
    , m_tick(GetTickCount64())
{
}

void glTFTimer::RecordFrameBegin()
{
    const size_t now = GetTickCount64();
    m_deltaTick = now - m_tick;
    m_tick = now;
}

size_t glTFTimer::GetDeltaFrameTimeMs() const
{
    return m_deltaTick;
}

glTFWindow& glTFWindow::Get()
{
    static glTFWindow window;
    return window;
}

bool glTFWindow::InitAndShowWindow()
{
    if (!glfwInit())
    {
        return false;
    }
    
    m_glfwWindow = glfwCreateWindow(m_width, m_height, "glTFRenderer v0.01      by jackjwang", NULL, NULL);
    if (!m_glfwWindow)
    {
        glfwTerminate();
        return false;
    }

    glfwSetKeyCallback(m_glfwWindow, KeyCallback);
    glfwSetMouseButtonCallback(m_glfwWindow, MouseButtonCallback);
    glfwSetCursorPosCallback(m_glfwWindow, CursorPosCallback);
    
    // Create test scene with box
    m_sceneGraph = std::make_unique<glTFSceneGraph>(); 
    //RETURN_IF_FALSE(LoadSceneGraphFromFile("glTFResources\\Models\\Box\\Box.gltf"))
    //RETURN_IF_FALSE(LoadSceneGraphFromFile("glTFResources\\Models\\Monster\\Monster.gltf"))
    RETURN_IF_FALSE(LoadSceneGraphFromFile("glTFResources\\Models\\Buggy\\glTF\\Buggy.gltf"))

    // Add camera
    std::unique_ptr<glTFSceneNode> cameraNode = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFCamera> camera = std::make_unique<glTFCamera>(cameraNode->m_transform,
        45.0f, 800.0f, 600.0f, 0.1f, 1000.0f);
    camera->Translate({0.0f, 1.0f, -50.0f});
    cameraNode->m_objects.push_back(std::move(camera));
    m_sceneGraph->AddSceneNode(std::move(cameraNode));

    // Add light
    std::unique_ptr<glTFSceneNode> directionalLightNode = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFDirectionalLight> directionalLight = std::make_unique<glTFDirectionalLight>(directionalLightNode->m_transform);
    directionalLight->Rotate({45.0f, 0.0f, 0.0f});
    directionalLight->SetIntensity(10.0f);
    directionalLight->SetTickFunc([lightNode = directionalLightNode.get()]()
    {
        lightNode->m_transform.m_rotation.y += 0.0001f;
        lightNode->m_transform.m_rotation.z += 0.0002f;
        lightNode->m_transform.m_rotation.x += 0.0003f;
        lightNode->renderStateDirty = true;
    });
    directionalLightNode->m_objects.push_back(std::move(directionalLight));

    std::unique_ptr<glTFSceneNode> pointLightNode = std::make_unique<glTFSceneNode>();
    std::unique_ptr<glTFPointLight> pointLight = std::make_unique<glTFPointLight>(pointLightNode->m_transform);
    pointLight->Translate({0.0f, 1.0f, 0.0f});
    pointLight->SetRadius(0.7f);
    pointLight->SetFalloff(1.0f);
    pointLight->SetIntensity(10000.0f);
    pointLightNode->m_objects.push_back(std::move(pointLight));

    m_sceneGraph->AddSceneNode(std::move(directionalLightNode));
    m_sceneGraph->AddSceneNode(std::move(pointLightNode));
    
    m_sceneView = std::make_unique<glTFSceneView>(*m_sceneGraph);
    
    if (!InitDX12())
    {
        return false;
    }

    return true;
}

void glTFWindow::UpdateWindow()
{
    while (!glfwWindowShouldClose(m_glfwWindow))
    {
        m_timer.RecordFrameBegin();
        const size_t deltaTimeInMs = m_timer.GetDeltaFrameTimeMs();
        
        m_sceneGraph->Tick(deltaTimeInMs);
        m_inputControl.TickSceneView(*m_sceneView, deltaTimeInMs);
        m_passManager->UpdateScene(deltaTimeInMs);
        m_passManager->RenderAllPass(deltaTimeInMs);

        glfwPollEvents();
    }

    m_passManager->ExitAllPass();
    glfwTerminate();
}

bool glTFWindow::InitDX12()
{
    m_passManager.reset(new glTFRenderPassManager(*this, *m_sceneView));
    m_passManager->AddRenderPass(std::make_unique<glTFRenderPassMeshOpaque>());
    m_passManager->AddRenderPass(std::make_unique<glTFRenderPassLighting>());
    m_passManager->InitAllPass();
    
    return true;
}

void glTFWindow::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        Get().m_inputControl.RecordKeyPressed(key);
    }
    else if (action == GLFW_RELEASE)
    {
        Get().m_inputControl.RecordKeyRelease(key);
    }
}

void glTFWindow::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        Get().m_inputControl.RecordMouseButtonPressed(button);
    }
    else if (action == GLFW_RELEASE)
    {
        Get().m_inputControl.RecordMouseButtonRelease(button);
    }
}

void glTFWindow::CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{   
    Get().m_inputControl.RecordCursorPos(xpos, ypos);
}

void glTFWindow::recursiveProcessChildrenNodes (const glTFLoader& loader, const glTFHandle& handle, const glTFSceneNode& parentNode, glTFSceneNode& sceneNode)
{
    const auto& node = loader.m_nodes[loader.ResolveIndex(handle)];
    sceneNode.m_transform = node->transform.m_matrix;
    sceneNode.m_finalTransform = parentNode.m_finalTransform.m_matrix * sceneNode.m_transform.m_matrix; 

    for (const auto& mesh_handle : node->meshes)
    {
        if (mesh_handle.IsValid())
        {
            const auto& mesh = *loader.m_meshes[loader.ResolveIndex(mesh_handle)];
            for (const auto& primitive : mesh.primitives)
            {
                VertexLayoutDeclaration vertexLayout;
                size_t vertexBufferSize = 0;

                std::vector<char*> vertexDataInGLTFBuffers;
            
                // POSITION attribute
                auto itPosition = primitive.attributes.find(glTF_Attribute_POSITION::attribute_type_id);
                if (itPosition != primitive.attributes.end())
                {
                    const glTFHandle accessorHandle = itPosition->second; 
                    const auto& vertexAccessor = *loader.m_accessors[loader.ResolveIndex(accessorHandle)];
                    vertexLayout.elements.push_back({VertexLayoutType::POSITION, vertexAccessor.GetElementByteSize()});
                    vertexBufferSize += vertexAccessor.count * vertexAccessor.GetElementByteSize();

                    const auto& vertexBufferView = *loader.m_bufferViews[loader.ResolveIndex(vertexAccessor.buffer_view)];
                    glTFHandle tempVertexBufferViewHandle = vertexBufferView.buffer;
                    tempVertexBufferViewHandle.node_index = loader.ResolveIndex(vertexBufferView.buffer);
                    auto findIt = loader.m_bufferDatas.find(tempVertexBufferViewHandle);
                    GLTF_CHECK(findIt != loader.m_bufferDatas.end());
                    char* bufferStart = findIt->second.get();
                    vertexDataInGLTFBuffers.push_back(bufferStart + vertexBufferView.byte_offset + vertexAccessor.byte_offset);
                }

                // NORMAL attribute
                auto itNormal = primitive.attributes.find(glTF_Attribute_NORMAL::attribute_type_id);
                if (itNormal != primitive.attributes.end())
                {
                    const glTFHandle accessorHandle = itNormal->second; 
                    const auto& vertexAccessor = *loader.m_accessors[loader.ResolveIndex(accessorHandle)];
                    vertexLayout.elements.push_back({VertexLayoutType::NORMAL, vertexAccessor.GetElementByteSize()});
                    vertexBufferSize += vertexAccessor.count * vertexAccessor.GetElementByteSize();

                    const auto& vertexBufferView = *loader.m_bufferViews[loader.ResolveIndex(vertexAccessor.buffer_view)];
                    glTFHandle tempVertexBufferViewHandle = vertexBufferView.buffer;
                    tempVertexBufferViewHandle.node_index = loader.ResolveIndex(vertexBufferView.buffer);
                    auto findIt = loader.m_bufferDatas.find(tempVertexBufferViewHandle);
                    GLTF_CHECK(findIt != loader.m_bufferDatas.end());
                    char* bufferStart = findIt->second.get();
                    vertexDataInGLTFBuffers.push_back(bufferStart + vertexBufferView.byte_offset + vertexAccessor.byte_offset);
                }

                std::shared_ptr<VertexBufferData> vertexBufferData = std::make_shared<VertexBufferData>();
                vertexBufferData->data.reset(new char[vertexBufferSize]);
                vertexBufferData->byteSize = vertexBufferSize;
                vertexBufferData->vertexCount = vertexBufferSize /vertexLayout.GetVertexStride();
                char* vertexDataStart = vertexBufferData->data.get();
                for (size_t v = 0; v < vertexBufferData->vertexCount; ++v)
                {
                    // Reformat vertex buffer data
                    for (size_t i = 0; i < vertexLayout.elements.size(); ++i)
                    {
                        memcpy(vertexDataStart, vertexDataInGLTFBuffers[i], vertexLayout.elements[i].byteSize);
                        vertexDataInGLTFBuffers[i] += vertexLayout.elements[i].byteSize;
                        vertexDataStart += vertexLayout.elements[i].byteSize;
                    }
                }
            
                const auto& indexAccessor = *loader.m_accessors[loader.ResolveIndex(primitive.indices)];
                const auto& indexBufferView = *loader.m_bufferViews[loader.ResolveIndex(indexAccessor.buffer_view)];

                const size_t indexBufferSize = indexAccessor.GetElementByteSize() * indexAccessor.count;
                
                std::shared_ptr<IndexBufferData> indexBufferData = std::make_shared<IndexBufferData>();
                indexBufferData->data.reset(new char[indexBufferSize]);
                glTFHandle tempIndexBufferViewHandle = indexBufferView.buffer;
                tempIndexBufferViewHandle.node_index = loader.ResolveIndex(indexBufferView.buffer);
                auto findIt = loader.m_bufferDatas.find(tempIndexBufferViewHandle);
                GLTF_CHECK(findIt != loader.m_bufferDatas.end());
                const char* bufferStart = findIt->second.get() + indexBufferView.byte_offset + indexAccessor.byte_offset;
                memcpy(indexBufferData->data.get(), bufferStart, indexBufferSize);
                indexBufferData->byteSize = indexBufferSize;
                indexBufferData->indexCount = indexAccessor.count;
                indexBufferData->elementType = indexAccessor.component_type == EUnsignedShort ? IndexBufferElementType::UNSIGNED_SHORT : IndexBufferElementType::UNSIGNED_INT;   
            
                sceneNode.m_objects.push_back(std::make_unique<glTFSceneTriangleMesh>(sceneNode.m_finalTransform, vertexLayout, vertexBufferData, indexBufferData));
            }
        }
    }
    
    for (const auto& child : node->children)
    {
        std::unique_ptr<glTFSceneNode> childNode = std::make_unique<glTFSceneNode>();
        recursiveProcessChildrenNodes(loader, child, sceneNode, *childNode);
        sceneNode.m_children.push_back(std::move(childNode));
    }
}

bool glTFWindow::LoadSceneGraphFromFile(const char* filePath)
{
    glTFLoader loader;
    const bool loaded = loader.LoadFile(filePath);
    if (loaded)
    {
        loader.Print();    
    }

    const auto& sceneNode = loader.m_scenes[loader.default_scene];
    for (const auto& rootNode : sceneNode->root_nodes)
    {
        std::unique_ptr<glTFSceneNode> sceneRootNodes = std::make_unique<glTFSceneNode>();
        recursiveProcessChildrenNodes(loader, rootNode, m_sceneGraph->GetRootNode(), *sceneRootNodes);
        
        m_sceneGraph->AddSceneNode(std::move(sceneRootNodes));
    }
    
    return true;
}