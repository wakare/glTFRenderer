#include <memory>
#include <string>
#include "DemoApps/DemoTriangleApp.h"

#define REGISTER_DEMO_APP(demo_name, app_name) if (demo_name == #app_name) {demo = std::make_unique<app_name>();} 

int main(int argc, char* argv[])
{
    if (argc <= 1)
    {
        // No argument so cannot decide run which demo app!
        return 0;
    }

    // Process console arguments
    std::vector<std::string> params(argv + 1, argv + argc);

    std::unique_ptr<DemoBase> demo;
    std::string demo_name = argv[1];
    
    REGISTER_DEMO_APP(demo_name, DemoTriangleApp)

    demo->Run(params);
    
    return 0;
}
