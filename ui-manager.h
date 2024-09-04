#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp>
#include "application.h"
#include <imgui/imgui.h>

class UiManager {
public:
    static bool init(GLFWwindow* window, wgpu::Device device, wgpu::TextureFormat surfaceFormat, wgpu::TextureFormat depthTextureFrmat);
    static void update(wgpu::RenderPassEncoder renderPass, Application::GlobalUniforms& globalUniforms, Application::LightingUniforms& lightingUniforms, bool& lightingUniFormsChanged);
    static void shutdown();
};