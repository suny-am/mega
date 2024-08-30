#pragma once
#include "webgpu/webgpu.hpp"
#include <glm/glm.hpp>

struct GLFWwindow;

class Application {
public:
    bool onInit();
    void onFrame();
    void onFinish();
    bool isRunning();

private:
    bool initWindowAndDevice();
    void terminateWindowAndDevice();

    bool initSurfaceConfiguration();
    void terminateSurfaceConfiguration();

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
};
