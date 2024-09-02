#pragma once
#include "webgpu/webgpu.hpp"
#include <glm/glm.hpp>
#include <filesystem>

using namespace wgpu;

struct GLFWwindow;

class Application {
public:
    using path = std::filesystem::path;

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
    void terminateBindGroupLayout();

    bool initRenderPipeline();
    void terminateRenderPipeline();

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
    void terminateBindGroup();

    void updateProjectionMatrix();
    void updateViewMatrix();
    void updateDragInertia();

    TextureView getNextSurfaceTextureView();

    bool initGui();
    void terminateGui();
    void updateGui(RenderPassEncoder renderPass);

    bool createWindow();

private:
    using mat4x4 = glm::mat4x4;
    using vec4 = glm::vec4;
    using vec3 = glm::vec3;
    using vec2 = glm::vec2;

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

    GLFWwindow* m_window = nullptr;
    Instance m_instance = nullptr;
    Surface m_surface = nullptr;
    Device m_device = nullptr;
    Queue m_queue = nullptr;
    TextureFormat m_surfaceFormat = TextureFormat::Undefined;
    // Keep the error callback alive
    std::unique_ptr<ErrorCallback> m_errorCallbackHandle;

    TextureFormat m_depthTextureFormat = TextureFormat::Depth24Plus;
    Texture m_depthTexture = nullptr;
    TextureView m_depthTextureView = nullptr;

    BindGroupLayout m_bindGroupLayout = nullptr;
    ShaderModule m_shaderModule = nullptr;
    RenderPipeline m_pipeline = nullptr;

    Sampler m_sampler = nullptr;
    Texture m_baseColorTexture = nullptr;
    TextureView m_baseColorTextureView = nullptr;
    Texture m_normalTexture = nullptr;
    TextureView m_normalTextureView = nullptr;

    Buffer m_vertexBuffer = nullptr;
    int m_vertexCount = 0;

    Buffer m_uniformBuffer = nullptr;
    SharedUniforms m_uniforms;

    Buffer m_lightingUniformBuffer = nullptr;
    LightingUniforms m_lightingUniforms;
    bool m_lightningUniformsChanged = false;

    BindGroup m_bindGroup = nullptr;

    CameraState m_cameraState;
    DragState m_drag;
};
