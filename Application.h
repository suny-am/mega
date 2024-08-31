#pragma once
#include "webgpu/webgpu.hpp"
#include <glm/glm.hpp>

struct GLFWwindow;

class Application {
public:
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

    bool initRenderPipeline();
    void terminateRenderPipeline();

    bool initTexture();
    void terminateTexture();

    bool initGeometry();
    void terminateGeometry();

    bool initUniforms();
    void terminateUniforms();

    bool initBindGroup();
    void terminateBindGroup();

    void updateProjectionMatrix();
    void updateViewMatrix();

    void updateDragInertia();

    wgpu::TextureView getNextSurfaceTextureView();

private:
    using mat4x4 = glm::mat4x4;
    using vec4 = glm::vec4;
    using vec3 = glm::vec3;
    using vec2 = glm::vec2;

    struct SharedUniforms {
        mat4x4 projectionMatrix;
        mat4x4 viewMatrix;
        mat4x4 modelMatrix;
        vec4 color;
        float time;
        float _pad[3];
    };

    static_assert(sizeof(SharedUniforms) % 16 == 0);

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
        vec2 velocity = {0.0, 0.0};
        vec2 previousDelta;
        float inertia = 0.9f;
    };

    GLFWwindow* m_window = nullptr;
    wgpu::Instance m_instance = nullptr;
    wgpu::Surface m_surface = nullptr;
    wgpu::Device m_device = nullptr;
    wgpu::Queue m_queue = nullptr;
    wgpu::TextureFormat m_surfaceFormat = wgpu::TextureFormat::Undefined;
    // Keep the error callback alive
    std::unique_ptr<wgpu::ErrorCallback> m_errorCallbackHandle;

    wgpu::TextureFormat m_depthTextureFormat = wgpu::TextureFormat::Depth24Plus;
    wgpu::Texture m_depthTexture = nullptr;
    wgpu::TextureView m_depthTextureView = nullptr;

    wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
    wgpu::ShaderModule m_shaderModule = nullptr;
    wgpu::RenderPipeline m_pipeline = nullptr;

    wgpu::Sampler m_sampler = nullptr;
    wgpu::Texture m_texture = nullptr;
    wgpu::TextureView m_textureView = nullptr;

    wgpu::Buffer m_vertexBuffer = nullptr;
    int m_vertexCount = 0;

    wgpu::Buffer m_uniformBuffer = nullptr;
    SharedUniforms m_uniforms;

    wgpu::BindGroup m_bindGroup = nullptr;

    CameraState m_cameraState;
    DragState m_drag;
};
