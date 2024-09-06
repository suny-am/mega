#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp>
#include "application.h"
#include <imgui/imgui.h>

class UiManager {
public:
    static bool init(GLFWwindow* window, wgpu::Device device, wgpu::TextureFormat surfaceFormat, wgpu::TextureFormat depthTextureFrmat);
    
    static void update(wgpu::RenderPassEncoder renderPass,
                       Application::GlobalUniforms& globalUniforms,
                       Application::LightingUniforms& lightingUniforms,
                       bool& lightingUniFormsChanged,
                       ResourceManager::path& filePath,
                       bool& filePathHasChanged);
    
    static void shutdown();

private:
    static void fileMenu(ResourceManager::path& filePath, bool& filePathHasChanged);
    
    static void lightingMenu(Application::GlobalUniforms& globalUniforms,
                  Application::LightingUniforms& lightingUniforms,
                  bool& lightingUniFormsChanged);
};