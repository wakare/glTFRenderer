#pragma once
#include <functional>
#include <memory>
#include <vector>
#include <string>

// Level0 -- max cover size, means lowest mip data 
class QuadTreeNode
{
public:
    QuadTreeNode(int x, int y, int width, int height, int level);
    
    bool Contains(int x, int y) const;
    bool Touch(int page_pixel_x, int page_pixel_y, int physical_x, int physical_y, int level);
    bool IsLeaf() const;
    bool IsValid() const;

    int GetX() const;
    int GetY() const;
    int GetWidth() const;
    int GetHeight() const;

    int GetPageX() const;
    int GetPageY() const;
    int GetLevel() const;
    
    void SetValid(bool valid, bool recursive);
    void TraverseLambda(std::function<void(const QuadTreeNode&)> callback, bool recursive) const;
    std::string ToString() const;
    
protected:
    bool Valid;
    int X;
    int Y;
    int Width;
    int Height;

    int PageX;
    int PageY;
    int Level;

    std::vector<std::shared_ptr<QuadTreeNode>> m_children;
};

class VTQuadTree
{
public:
    bool InitQuadTree(int page_table_size, int page_size);
    bool Touch(int page_x, int page_y, int physical_offset_x, int physical_offset_y, int level);
    void Invalidate();
    void TraverseLambda(std::function<void(const QuadTreeNode&)> callback) const;
    
    int GetLevel(int mip) const;
    
protected:
    int m_max_mip_level {0};
    int m_page_size {0};

    std::shared_ptr<QuadTreeNode> m_root;
};
