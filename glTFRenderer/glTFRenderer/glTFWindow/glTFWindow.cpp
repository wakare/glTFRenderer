#include "glTFWindow.h"
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3native.h>

#include "../glTFLight/glTFDirectionalLight.h"
#include "../glTFLight/glTFPointLight.h"
#include "../glTFMaterial/glTFMaterialOpaque.h"
#include "../glTFRenderPass/glTFRenderPassLighting.h"
#include "../glTFRenderPass/glTFRenderPassMeshOpaque.h"
#include "../glTFScene/glTFSceneBox.h"
#include "../glTFScene/glTFCamera.h"
#include "../glTFLoader/glTFLoader.h"
#include "../glTFScene/glTFSceneTriangleMesh.h"

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

bool CreateTestScene(glTFSceneGraph& sceneGraph)
{
    std::shared_ptr<glTFMaterialOpaque> boxAlbedoMaterial = std::make_shared<glTFMaterialOpaque>(std::make_shared<glTFMaterialTexture>("glTFResources/tiger.bmp"));
    
    std::unique_ptr<glTFSceneNode> boxSceneNode = std::make_unique<glTFSceneNode>();
    boxSceneNode->object = std::make_unique<glTFSceneBox>();
    boxSceneNode->object->GetTransform().position = {5.0f, 0.0f, 0.0f};
    auto* boxNode = boxSceneNode.get();
    boxNode->object->SetTickFunc([boxNode]()
    {
        auto* box = boxNode->object.get();
        box->GetTransform().rotation.x += 0.0001f;
        box->GetTransform().rotation.y += 0.0002f;
        box->GetTransform().rotation.z += 0.0003f;
        boxNode->renderStateDirty = true;
    });
    ((glTFSceneBox*)boxSceneNode->object.get())->SetMaterial(boxAlbedoMaterial);
    sceneGraph.AddSceneNode(std::move(boxSceneNode));

    std::unique_ptr<glTFSceneNode> boxSceneNode2 = std::make_unique<glTFSceneNode>();
    boxSceneNode2->object = std::make_unique<glTFSceneBox>();
    boxSceneNode2->object->GetTransform().position = {-5.0f, 0.0f, 0.0f};
    auto* boxNode2 = boxSceneNode2.get();
    boxNode2->object->SetTickFunc([boxNode2]()
    {
        auto* box = boxNode2->object.get();
        box->GetTransform().rotation.x += 0.0001f;
        box->GetTransform().rotation.y += 0.0002f;
        box->GetTransform().rotation.z += 0.0003f;
        boxNode2->renderStateDirty = true;
    });
    ((glTFSceneBox*)boxSceneNode2->object.get())->SetMaterial(boxAlbedoMaterial);
    sceneGraph.AddSceneNode(std::move(boxSceneNode2));
    
    return true;
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
    glfwSetCursorPosCallback(m_glfwWindow, CursorPosCallback);
    
    // Create test scene with box
    m_sceneGraph = std::make_unique<glTFSceneGraph>(); 
    RETURN_IF_FALSE(LoadSceneGraphFromFile("D:\\Work\\DevSpace\\glTFRenderer\\glTFRenderer\\glTFRenderer\\glTFRenderer\\glTFResources\\Models\\Box\\Box.gltf"))

    // Add camera
    std::unique_ptr<glTFCamera> camera = std::make_unique<glTFCamera>(45.0f, 800.0f, 600.0f, 0.1f, 1000.0f);
    camera->GetTransform() = glTFTransform::Identity();
    camera->GetTransform().position = {0.0f, 1.0f, -5.0f};

    std::unique_ptr<glTFSceneNode> cameraNode = std::make_unique<glTFSceneNode>();
    cameraNode->object = std::move(camera);
    m_sceneGraph->AddSceneNode(std::move(cameraNode));

    // Add light
    std::unique_ptr<glTFDirectionalLight> directionalLight = std::make_unique<glTFDirectionalLight>();
    directionalLight->GetTransform().rotation = {45.0f, 0.0f, 0.0f};
    directionalLight->SetIntensity(10.0f);
    
    std::unique_ptr<glTFSceneNode> directionalLightNode = std::make_unique<glTFSceneNode>();
    directionalLight->SetTickFunc([lightNode = directionalLightNode.get()]()
    {
        lightNode->object->GetTransform().rotation.y += 0.0001f;
        lightNode->object->GetTransform().rotation.z += 0.0002f;
        lightNode->object->GetTransform().rotation.x += 0.0003f;
        lightNode->renderStateDirty = true;
    });
    directionalLightNode->object = std::move(directionalLight);

    std::unique_ptr<glTFPointLight> pointLight = std::make_unique<glTFPointLight>();
    pointLight->GetTransform().position = {0.0f, 1.0f, 0.0f};
    pointLight->SetRadius(0.7f);
    pointLight->SetFalloff(1.0f);
    pointLight->SetIntensity(10000.0f);

    std::unique_ptr<glTFSceneNode> pointLightNode = std::make_unique<glTFSceneNode>();
    pointLightNode->object = std::move(pointLight);

    //m_sceneGraph->AddSceneNode(std::move(directionalLightNode));
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

void glTFWindow::CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{   
    Get().m_inputControl.RecordCursorPos(xpos, ypos);
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
        const auto& node = loader.m_nodes[rootNode];
        std::unique_ptr<glTFSceneNode> newNode = std::make_unique<glTFSceneNode>();
        const auto& Mesh = *loader.m_meshes[node->mesh];
        for (const auto& primitive : Mesh.primitives)
        {
            VertexLayoutDeclaration vertexLayout;
            size_t vertexBufferSize = 0;

            std::vector<char*> vertexDataInGLTFBuffers;
            
            // POSITION attribute
            auto itPosition = primitive.attributes.find(glTF_Attribute_POSITION::attribute_type_id);
            if (itPosition != primitive.attributes.end())
            {
                const glTFHandle accessorHandle = itPosition->second; 
                const auto& vertexAccessor = *loader.m_accessors[accessorHandle];
                vertexLayout.elements.push_back({VertexLayoutType::POSITION, vertexAccessor.GetElementByteSize()});
                vertexBufferSize += vertexAccessor.count * vertexAccessor.GetElementByteSize();

                const auto& vertexBufferView = *loader.m_bufferViews[vertexAccessor.buffer_view];
                char* bufferStart = loader.m_bufferDatas[vertexBufferView.buffer].get();
                vertexDataInGLTFBuffers.push_back(bufferStart + vertexBufferView.byte_offset + vertexAccessor.byte_offset);
            }

            // NORMAL attribute
            auto itNormal = primitive.attributes.find(glTF_Attribute_NORMAL::attribute_type_id);
            if (itNormal != primitive.attributes.end())
            {
                const glTFHandle accessorHandle = itNormal->second; 
                const auto& vertexAccessor = *loader.m_accessors[accessorHandle];
                vertexLayout.elements.push_back({VertexLayoutType::NORMAL, vertexAccessor.GetElementByteSize()});
                vertexBufferSize += vertexAccessor.count * vertexAccessor.GetElementByteSize();

                const auto& vertexBufferView = *loader.m_bufferViews[vertexAccessor.buffer_view];
                char* bufferStart =  loader.m_bufferDatas[vertexBufferView.buffer].get();
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
            
            const auto& indexAccessor = *loader.m_accessors[primitive.indices];
            const auto& indexBufferView = *loader.m_bufferViews[indexAccessor.buffer_view];
            
            std::shared_ptr<IndexBufferData> indexBufferData = std::make_shared<IndexBufferData>();
            indexBufferData->data.reset(new char[indexBufferView.byte_length]);
            memcpy(indexBufferData->data.get(), loader.m_bufferDatas[indexBufferView.buffer].get() + indexBufferView.byte_offset + indexAccessor.byte_offset, indexBufferView.byte_length);
            indexBufferData->byteSize = indexBufferView.byte_length;
            indexBufferData->indexCount = indexAccessor.count;
            indexBufferData->elementType = indexAccessor.component_type == EUnsignedShort ? IndexBufferElementType::UNSIGNED_SHORT : IndexBufferElementType::UNSIGNED_INT;   
            
            newNode->object.reset(new glTFSceneTriangleMesh(vertexLayout, vertexBufferData, indexBufferData));
        }

        m_sceneGraph->AddSceneNode(std::move(newNode));
    }
    
    return true;
}
