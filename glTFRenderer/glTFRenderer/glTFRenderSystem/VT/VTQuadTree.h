#pragma once
#include <memory>
#include <vector>

// Level0 -- max cover size, means lowest mip data 
class QuadTreeNode
{
public:
    QuadTreeNode(int x, int y, int width, int height, int level);
    
    bool Contains(int x, int y) const;
    bool Touch(int x, int y, int level);
    bool IsLeaf() const;
    
protected:
    
    int X;
    int Y;
    int Width;
    int Height;
    int Level;

    std::vector<std::shared_ptr<QuadTreeNode>> m_children;
};

class VTQuadTree
{
public:
    bool InitQuadTree(int page_table_size, int page_size);
    bool Touch(int x, int y, int level);
    int GetLevel(int mip) const;
    
protected:
    int m_max_mip_level {0};
    int m_page_size {0};

    std::shared_ptr<QuadTreeNode> m_root;
};
