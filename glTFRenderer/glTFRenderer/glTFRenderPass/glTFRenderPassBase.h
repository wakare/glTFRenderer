#pragma once

class IRHICommandList;

class glTFRenderPassBase 
{
public:
    virtual bool InitRenderPass() = 0;
    virtual bool RenderPass(IRHICommandList& commandList) = 0;
};
