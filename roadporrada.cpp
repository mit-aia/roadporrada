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
    init.type = bgfx::RendererType::Direct3D12; // Try D3D12 first (RTX 4090 supports it)
                                                // init.type = bgfx::RendererType::Direct3D11; // Fallback to D3D11
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

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
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

// // Try different renderers explicitly
// bgfx::Init init;
// init.type = bgfx::RendererType::Direct3D12; // Try D3D12 first (RTX 4090 supports it)
// // init.type = bgfx::RendererType::Direct3D11; // Fallback to D3D11
// // init.type = bgfx::RendererType::Vulkan;     // Or Vulkan
// init.resolution.width = 800;
// init.resolution.height = 600;
// init.resolution.reset = BGFX_RESET_VSYNC;
// // Set debug mode to see more info
// init.debug = true;
// init.profile = true;
// printf("Attempting to initialize bgfx with type: %s\n",
//        bgfx::getRendererName(init.type));
// if (!bgfx::init(init))
// {
//     fprintf(stderr, "Failed to initialize bgfx with renderer: %s\n",
//             bgfx::getRendererName(init.type));

//     // Try again with D3D11 as fallback
//     init.type = bgfx::RendererType::Direct3D11;
//     printf("Retrying with D3D11...\n");
//     if (!bgfx::init(init))
//     {
//         fprintf(stderr, "Failed to initialize bgfx with D3D11 as well\n");
//         return 1;
//     }
// }