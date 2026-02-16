// #include <bgfx/bgfx.h>
// #include <bgfx/platform.h>
// #include <GLFW/glfw3.h>
// #include <cstdio>
#include <iostream>
using namespace std;

int main()
{
    std::cout << "Hello World" << std::endl;
    std::cout << "I am learning C++" << std::endl;

    // if (!glfwInit()) {
    //     fprintf(stderr, "Failed to initialize GLFW\n");
    //     return 1;
    // }

    // glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // GLFWwindow* window = glfwCreateWindow(800, 600, "RoadPorrada", nullptr, nullptr);
    // if (!window) {
    //     fprintf(stderr, "Failed to create GLFW window\n");
    //     glfwTerminate();
    //     return 1;
    // }

    // bgfx::PlatformData pd;
    // pd.nwh = (void*)(uintptr_t)glfwGetWin32Window(window);
    // bgfx::setPlatformData(pd);

    // bgfx::Init init;
    // init.resolution.width = 800;
    // init.resolution.height = 600;
    // init.resolution.reset = BGFX_RESET_VSYNC;
    // if (!bgfx::init(init)) {
    //     fprintf(stderr, "Failed to initialize bgfx\n");
    //     glfwDestroyWindow(window);
    //     glfwTerminate();
    //     return 1;
    // }

    // bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    // bgfx::setViewRect(0, 0, 0, 800, 600);

    // printf("GLFW and bgfx initialized successfully!\n");
    // printf("Press any key in the console to exit...\n");

    // while (!glfwWindowShouldClose(window)) {
    //     glfwPollEvents();

    //     bgfx::touch(0);
    //     bgfx::frame();

    //     if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    //         glfwSetWindowShouldClose(window, GLFW_TRUE);
    //     }
    // }

    // bgfx::shutdown();
    // glfwDestroyWindow(window);
    // glfwTerminate();

    return 0;
}
