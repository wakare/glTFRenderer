#include "RendererSceneAABB.h"

#include <glm/glm/gtx/component_wise.hpp>

RendererSceneAABB::RendererSceneAABB()
{
  setNull();
}

RendererSceneAABB::RendererSceneAABB(const glm::vec3& center, float radius)
{
  setNull();
  extend(center, radius);
}

RendererSceneAABB::RendererSceneAABB(const glm::vec3& p1, const glm::vec3& p2)
{
  setNull();
  extend(p1);
  extend(p2);
}

RendererSceneAABB::RendererSceneAABB(const RendererSceneAABB& RenderSceneAABB)
{
  setNull();
  extend(RenderSceneAABB);
}

RendererSceneAABB::~RendererSceneAABB()
{
}

void RendererSceneAABB::extend(float val)
{
  if (!isNull())
  {
    mMin -= glm::vec3(val);
    mMax += glm::vec3(val);
  }
}

void RendererSceneAABB::extend(const glm::vec3& p)
{
  if (!isNull())
  {
    mMin = glm::min(p, mMin);
    mMax = glm::max(p, mMax);
  }
  else
  {
    mMin = p;
    mMax = p;
  }
}

void RendererSceneAABB::extend(const glm::vec3& p, float radius)
{
  glm::vec3 r(radius);
  if (!isNull())
  {
    mMin = glm::min(p - r, mMin);
    mMax = glm::max(p + r, mMax);
  }
  else
  {
    mMin = p - r;
    mMax = p + r;
  }
}

void RendererSceneAABB::extend(const RendererSceneAABB& RenderSceneAABB)
{
  if (!RenderSceneAABB.isNull())
  {
    extend(RenderSceneAABB.mMin);
    extend(RenderSceneAABB.mMax);
  }
}

void RendererSceneAABB::extendDisk(const glm::vec3& c, const glm::vec3& n, float r)
{
  if (glm::length(n) < 1.e-12) { extend(c); return; }
  glm::vec3 norm = glm::normalize(n);
  float x = sqrt(1 - norm.x) * r;
  float y = sqrt(1 - norm.y) * r;
  float z = sqrt(1 - norm.z) * r;
  extend(c + glm::vec3(x,y,z));
  extend(c - glm::vec3(x,y,z));
}

glm::vec3 RendererSceneAABB::getDiagonal() const
{
  if (!isNull())
    return mMax - mMin;
  else
    return glm::vec3(0);
}

float RendererSceneAABB::getLongestEdge() const
{
  return glm::compMax(getDiagonal());
}

float RendererSceneAABB::getShortestEdge() const
{
  return glm::compMin(getDiagonal());
}

glm::vec3 RendererSceneAABB::getCenter() const
{
  if (!isNull())
  {
    glm::vec3 d = getDiagonal();
    return mMin + (d * float(0.5));
  }
  else
  {
    return glm::vec3(0.0);
  }
}

void RendererSceneAABB::translate(const glm::vec3& v)
{
  if (!isNull())
  {
    mMin += v;
    mMax += v;
  }
}

void RendererSceneAABB::scale(const glm::vec3& s, const glm::vec3& o)
{
  if (!isNull())
  {
    mMin -= o;
    mMax -= o;

    mMin *= s;
    mMax *= s;

    mMin += o;
    mMax += o;
  }
}

bool RendererSceneAABB::overlaps(const RendererSceneAABB& bb) const
{
  if (isNull() || bb.isNull())
    return false;

  if( bb.mMin.x > mMax.x || bb.mMax.x < mMin.x)
    return false;
  else if( bb.mMin.y > mMax.y || bb.mMax.y < mMin.y)
    return false;
  else if( bb.mMin.z > mMax.z || bb.mMax.z < mMin.z)
    return false;

  return true;
}

RendererSceneAABB::INTERSECTION_TYPE RendererSceneAABB::intersect(const RendererSceneAABB& b) const
{
  if (isNull() || b.isNull())
    return OUTSIDE;

  if ((mMax.x < b.mMin.x) || (mMin.x > b.mMax.x) ||
      (mMax.y < b.mMin.y) || (mMin.y > b.mMax.y) ||
      (mMax.z < b.mMin.z) || (mMin.z > b.mMax.z)) 
  {
    return OUTSIDE;
  }

  if ((mMin.x <= b.mMin.x) && (mMax.x >= b.mMax.x) &&
      (mMin.y <= b.mMin.y) && (mMax.y >= b.mMax.y) &&
      (mMin.z <= b.mMin.z) && (mMax.z >= b.mMax.z)) 
  {
    return INSIDE;
  }

  return INTERSECT;    
}


bool RendererSceneAABB::isSimilarTo(const RendererSceneAABB& b, float diff) const
{
  if (isNull() || b.isNull()) return false;

  glm::vec3 acceptable_diff=( (getDiagonal()+b.getDiagonal()) / float(2.0))*diff;
  glm::vec3 min_diff(mMin-b.mMin);
  min_diff = glm::vec3(fabs(min_diff.x),fabs(min_diff.y),fabs(min_diff.z));
  if (min_diff.x > acceptable_diff.x) return false;
  if (min_diff.y > acceptable_diff.y) return false;
  if (min_diff.z > acceptable_diff.z) return false;
  glm::vec3 max_diff(mMax-b.mMax);
  max_diff = glm::vec3(fabs(max_diff.x),fabs(max_diff.y),fabs(max_diff.z));
  if (max_diff.x > acceptable_diff.x) return false;
  if (max_diff.y > acceptable_diff.y) return false;
  if (max_diff.z > acceptable_diff.z) return false;
  return true;
}

RendererSceneAABB RendererSceneAABB::TransformAABB(const glm::mat4& mat, const RendererSceneAABB& input)
{
  glm::vec4 cornerMin = {input.getMin(), 1.0f};
  glm::vec4 cornerMax = {input.getMax(), 1.0f};

  cornerMin = mat * cornerMin;
  cornerMax = mat * cornerMax;

  return {cornerMin, cornerMax};
}