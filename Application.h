#pragma once
#include "webgpu/webgpu.hpp"
#include "webgpu-raii.hpp"
#include <glm/glm.hpp>
#include <filesystem>

using namespace wgpu;

struct GLFWwindow;

class Application {
public:
    using path = std::filesystem::path;
    using vec2 = glm::vec2;

    struct CameraState {
        vec2 angles = { 0.8f, 0.5f };
        float zoom = -1.2f;
    };

    struct DragState {
        bool active = false;
        vec2 startPos;
        CameraState startCameraState;
        float sensitivity = 0.01f;
        float scrollSensitivity = 0.1f;
        vec2 velocity = { 0.0, 0.0 };
        vec2 previousDelta;
        float inertia = 0.9f;
    };

    bool onInit();
    void onFrame();
    void onResize();
    void onFinish();
    bool isRunning();

    void onMouseMove(double xPos, double yPos);
    void onMouseButton(int button, int action, int mods);
    void onScroll(double xOffset, double yOffset);

private:
    bool initWindowAndDevice();
    void terminateWindowAndDevice();

    bool initSurfaceConfiguration();

    bool initDepthBuffer();
    void terminateDepthBuffer();

    bool initBindGroupLayout();

    bool initRenderPipeline();

    bool initTextures();
    void terminateTextures();

    bool initGeometry(const path& path);
    void updateGeometry(const path& path);
    void terminateGeometry();

    bool initUniforms();
    void terminateUniforms();

    bool initLightingUniforms();
    void terminateLightingUniforms();
    void updateLightingUniforms();

    bool initBindGroup();

    void updateProjectionMatrix();
    void updateViewMatrix();

    TextureView getNextSurfaceTextureView();

    bool initGui();
    void terminateGui();
    void updateGui(RenderPassEncoder renderPass);

    bool createWindow();

private:
    using mat4x4 = glm::mat4x4;
    using vec4 = glm::vec4;
    using vec3 = glm::vec3;

    struct SharedUniforms {
        mat4x4 projectionMatrix;
        mat4x4 viewMatrix;
        mat4x4 modelMatrix;
        vec4 worldColor;
        vec4 objectColor;
        vec3 cameraWorldPosition;
        float time;
    };
    static_assert(sizeof(SharedUniforms) % 16 == 0);

    struct LightingUniforms {
        std::array<vec4, 2> directions;
        std::array<vec4, 2> colors;
        float hardness = 32.0f;
        float kd = 1.0f;
        float ks = 0.5f;
        float kn = 0.5f;
    };
    static_assert(sizeof(LightingUniforms) % 16 == 0);

    GLFWwindow* m_window = nullptr;
    raii::Instance m_instance;
    raii::Surface m_surface;
    raii::Device m_device;
    raii::Queue m_queue;
    TextureFormat m_surfaceFormat = TextureFormat::Undefined;
    // Keep the error callback alive
    std::unique_ptr<ErrorCallback> m_errorCallbackHandle;

    TextureFormat m_depthTextureFormat = TextureFormat::Depth24Plus;
    raii::Texture m_depthTexture;
    raii::TextureView m_depthTextureView;

    raii::BindGroupLayout m_bindGroupLayout;
    raii::ShaderModule m_shaderModule;
    raii::RenderPipeline m_pipeline;

    raii::Sampler m_sampler;
    raii::Texture m_baseColorTexture;
    raii::TextureView m_baseColorTextureView;
    raii::Texture m_normalTexture;
    raii::TextureView m_normalTextureView;

    raii::Buffer m_vertexBuffer;
    int m_vertexCount = 0;

    raii::Buffer m_uniformBuffer;
    SharedUniforms m_uniforms;

    raii::Buffer m_lightingUniformBuffer;
    LightingUniforms m_lightingUniforms;
    bool m_lightningUniformsChanged = false;

    raii::BindGroup m_bindGroup;

    CameraState m_cameraState;
    DragState m_drag;
};
