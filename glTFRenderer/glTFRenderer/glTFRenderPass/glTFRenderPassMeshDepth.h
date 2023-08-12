#pragma once
#include "glTFRenderPassMeshBase.h"

class glTFRenderPassMeshDepth : public glTFRenderPassMeshBase
{
public:
    virtual const char* PassName() override {return "MeshPassOpaque"; }

private:
};
