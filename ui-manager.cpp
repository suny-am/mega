#include "ui-manager.h"
#include "gpu-scene.h"
#include <glm/glm/glm.hpp>
#include <glm/glm/ext.hpp>
#include <imgui/imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm/gtx/polar_coordinates.hpp>

namespace ImGui {
    bool DragDirection(const char* label, glm::vec4& direction) {
        glm::vec2 angles = glm::degrees(glm::polar(glm::vec3(direction)));
        bool changed = ImGui::DragFloat2(label, glm::value_ptr(angles));
        direction = glm::vec4(glm::euclidean(glm::radians(angles)), direction.w);
        return changed;
    }
}

bool UiManager::init(GLFWwindow* window, wgpu::Device device, wgpu::TextureFormat surfaceFormat, wgpu::TextureFormat depthTextureFormat) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();

    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplWGPU_InitInfo initInfo;

    initInfo.Device = device;
    initInfo.NumFramesInFlight = 3;
    initInfo.RenderTargetFormat = surfaceFormat;
    initInfo.DepthStencilFormat = depthTextureFormat;

    ImGui_ImplWGPU_Init(&initInfo);
    return true;
}

void UiManager::update(wgpu::RenderPassEncoder renderPass,
                       Application::GlobalUniforms& globalUniforms,
                       Application::LightingUniforms& lightingUniforms,
                       bool& lightingUniFormsChanged) {
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
        bool changed = false;
        // TBD File
        auto io = ImGui::GetIO();
        ImGui::SetWindowPos(ImVec2(io.DisplaySize.x / 2 - ImGui::GetWindowWidth() / 2, 0));
        ImGui::Begin("Lighting");
        changed = ImGui::ColorEdit3("World", glm::value_ptr(globalUniforms.worldColor)) || changed;
        changed = ImGui::ColorEdit3("Color #0", glm::value_ptr(lightingUniforms.colors[0])) || changed;
        changed = ImGui::DragDirection("Direction #0", lightingUniforms.directions[0]) || changed;
        changed = ImGui::ColorEdit3("Color #1", glm::value_ptr(lightingUniforms.colors[1])) || changed;
        changed = ImGui::DragDirection("Direction #1", lightingUniforms.directions[1]) || changed;
        ImGui::End();
        lightingUniFormsChanged = changed;
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
}

void UiManager::shutdown() {
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplWGPU_Shutdown();
}