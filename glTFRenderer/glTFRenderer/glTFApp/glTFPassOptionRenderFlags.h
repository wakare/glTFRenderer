#pragma once
#include "RendererCommon.h"

enum glTFPassOptionFlags
{
    glTFPassOptionFlags_Lit = 1 << 0,
    glTFPassOptionFlags_Culling = 1 << 1,
    glTFPassOptionFlags_Shadow = 1 << 2,
};

#define ADD_RENDER_FLAGS(value)\
void SetEnable##value(bool enable)\
{\
if (enable)\
{\
SetFlag(glTFPassOptionFlags_##value);\
}\
else\
{\
UnsetFlag(glTFPassOptionFlags_##value);\
}\
}\
bool Is##value() const {return IsFlagSet(glTFPassOptionFlags_##value); }

struct glTFPassOptionRenderFlags : public glTFFlagsBase<glTFPassOptionFlags>
{
    ADD_RENDER_FLAGS(Lit)
    ADD_RENDER_FLAGS(Culling)
    ADD_RENDER_FLAGS(Shadow)
};