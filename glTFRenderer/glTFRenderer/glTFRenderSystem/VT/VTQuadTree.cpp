#include "VTQuadTree.h"

#include <cmath>

#include "RendererCommon.h"

static bool ContainsPoint(int x, int y, int width, int height, int px, int py)
{
    return
        x <= px && px < x + width &&
        y <= py && py < y + height;
}

QuadTreeNode::QuadTreeNode(int x, int y, int width, int height, int level)
    : Valid(true), X(x), Y(y), Width(width), Height(height), PageX(-1), PageY(-1), Level(level)
{
}

bool QuadTreeNode::Contains(int x, int y) const
{
    return ContainsPoint(X, Y, Width, Height, x, y);
}

bool QuadTreeNode::Touch(int x, int y, int page_x, int page_y, int level)
{
    if (!Contains(x, y))
    {
        return false;
    }

    if (level == Level)
    {
        PageX = page_x;
        PageY = page_y;
        return true;
    }

    // Mark valid
    SetValid(true, false);

    // Allocate child
    int child_width = Width / 2;
    int child_height = Height / 2;

    int child_bounds[4][4] =
        {
        {X, Y, child_width, child_height},
        {X + child_width, Y, child_width, child_height},
        {X, Y + child_height, child_width, child_height},
        {X + child_width, Y + child_height, child_width, child_height}
        };

    std::shared_ptr<QuadTreeNode> child_node = nullptr;
    for (int i = 0; i < 4; i++)
    {
        if (ContainsPoint(child_bounds[i][0], child_bounds[i][1], child_bounds[i][2], child_bounds[i][3], x, y))
        {
            child_node = std::make_shared<QuadTreeNode>(child_bounds[i][0], child_bounds[i][1], child_bounds[i][2], child_bounds[i][3], Level + 1);
            m_children.push_back(child_node);
            break;
        }
    }

    if (!child_node)
    {
        GLTF_CHECK(false);
        return false;
    }
    
    return child_node->Touch(x, y, page_x, page_y, level);
}

void QuadTreeNode::SetValid(bool valid, bool recursive)
{
    Valid = valid;
    if (recursive)
    {
        for (auto& child : m_children)
        {
            child->SetValid(valid, recursive);
        }    
    }
}

void QuadTreeNode::TraverseLambda(std::function<void(const QuadTreeNode&)> callback, bool recursive) const
{
    callback(*this);
    
    if (recursive)
    {
        for (auto& child : m_children)
        {
            child->TraverseLambda(callback, recursive);
        }
    }
}

bool QuadTreeNode::IsLeaf() const
{
    return m_children.empty();
}

bool QuadTreeNode::IsValid() const
{
    return Valid;
}

int QuadTreeNode::GetX() const
{
    return X;
}

int QuadTreeNode::GetY() const
{
    return Y;
}

int QuadTreeNode::GetWidth() const
{
    return Width;
}

int QuadTreeNode::GetHeight() const
{
    return Height;
}

int QuadTreeNode::GetPageX() const
{
    return PageX;
}

int QuadTreeNode::GetPageY() const
{
    return PageY;
}

int QuadTreeNode::GetLevel() const
{
    return Level;
}

bool VTQuadTree::InitQuadTree(int page_table_size, int page_size)
{
    m_max_mip_level = log2(page_table_size);
    m_page_size = page_size;
    int quad_tree_size = page_table_size * m_page_size;
    
    LOG_FORMAT_FLUSH("[VT QuadTree] Init max mip %d, page size %d, quadtree size %d\n", m_max_mip_level, m_page_size, quad_tree_size);

    m_root = std::make_shared<QuadTreeNode>(0 ,0, quad_tree_size, quad_tree_size, 0);
    
    return true;
}

bool VTQuadTree::Touch(int x, int y, int page_x, int page_y, int level)
{
    GLTF_CHECK(m_root != nullptr);
    return m_root->Touch(x, y, page_x, page_y, level);
}

void VTQuadTree::Invalidate()
{
    m_root->SetValid(false, true);   
}

void VTQuadTree::TraverseLambda(std::function<void(const QuadTreeNode&)> callback) const
{
    m_root->TraverseLambda(callback, true);
}

int VTQuadTree::GetLevel(int mip) const
{
    return m_max_mip_level - mip;
}
