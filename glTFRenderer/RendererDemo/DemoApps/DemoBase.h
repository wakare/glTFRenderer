#pragma once
#include <vector>
#include <string>

class DemoBase
{
public:
    virtual void Run(const std::vector<std::string>& arguments) = 0;
};
