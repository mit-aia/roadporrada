#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <cstdio>
#include <iostream>

int main()
{
    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(800, 600, "RoadPorrada", nullptr, nullptr);
    if (!window)
    {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    bgfx::PlatformData pd;
    pd.nwh = (void *)(uintptr_t)glfwGetWin32Window(window);
    bgfx::setPlatformData(pd);

    bgfx::Init init;
    init.platformData = pd;
    // init.type = bgfx::RendererType::Direct3D12;
    // init.type = bgfx::RendererType::Direct3D11;
    // init.type = bgfx::RendererType::Vulkan;

    init.resolution.width = 800;
    init.resolution.height = 600;
    // init.resolution.reset = BGFX_RESET_VSYNC;
    if (!bgfx::init(init))
    {
        fprintf(stderr, "Failed to initialize bgfx\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, 800, 600);

    printf("GLFW and bgfx initialized successfully!\n");
    printf("Press any key in the console to exit...\n");

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        bgfx::touch(0);
        bgfx::frame();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    bgfx::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

// #include "bgfx/bgfx.h"
// #include <bgfx/platform.h>
// #include "GLFW/glfw3.h"
// #define GLFW_EXPOSE_NATIVE_WIN32
// #include "GLFW/glfw3native.h"

// #define WNDW_WIDTH 1600
// #define WNDW_HEIGHT 900

// int main(void)
// {
//     glfwInit();
//     GLFWwindow *window = glfwCreateWindow(WNDW_WIDTH, WNDW_HEIGHT, "Hello, bgfx!", NULL, NULL);

//     bgfx::PlatformData pd;
//     pd.nwh = glfwGetWin32Window(window);
//     bgfx::setPlatformData(pd);

//     bgfx::Init bgfxInit;
//     bgfxInit.type = bgfx::RendererType::Count; // Automatically choose a renderer.
//     bgfxInit.resolution.width = WNDW_WIDTH;
//     bgfxInit.resolution.height = WNDW_HEIGHT;
//     bgfxInit.resolution.reset = BGFX_RESET_VSYNC;
//     bgfxInit.debug = true; // Enable debug mode for better error messages and debugging features.
//     bgfxInit.platformData = pd; // Pass the platform data to bgfx initialization.
//     bgfx::init(bgfxInit);

//     bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
//     bgfx::setViewRect(0, 0, 0, WNDW_WIDTH, WNDW_HEIGHT);

//     unsigned int counter = 0;
//     while (true)
//     {
//         bgfx::frame();
//         counter++;
//     }

//     return 0;
// }
