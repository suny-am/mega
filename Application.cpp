#include "Application.h"
#include "ResourceManager.h"
#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>
#include "glm/glm.hpp"
#include <glm/ext.hpp>
#include <iostream>
#include <cassert>
#include <filesystem>
#include <array>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <cstddef>

using std::cout;
using std::cerr;
using std::endl;
using std::vector;

using namespace wgpu;
using VertexAttributes = ResourceManager::VertexAttributes;

constexpr float PI = 3.14159265358979323846f;

bool Application::onInit() {
    if (!initWindowAndDevice()) return false;
    if (!initSurfaceConfiguration()) return false;
    if (!initDepthBuffer()) return false;
    if (!initRenderPipeline()) return false;
    if (!initTexture()) return false;
    if (!initGeometry()) return false;
    if (!initUniforms()) return false;
    if (!initBindGroup()) return false;
    return true;
}

void Application::onFrame() {
    updateDragInertia();
    glfwPollEvents();

    TextureView nextTexture = getNextSurfaceTextureView();
    if (!nextTexture) {
        cerr << "Could not acquire next texture from surface" << endl;
        return;
    }

    CommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = "My command encoder";
    CommandEncoder encoder = m_device.createCommandEncoder(encoderDesc);

    RenderPassDescriptor renderPassDesc = {};

    RenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = nextTexture;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = LoadOp::Clear;
    renderPassColorAttachment.storeOp = StoreOp::Store;
    renderPassColorAttachment.clearValue = Color{ 0.05, 0.05, 0.05, 1.0 };
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;

    RenderPassDepthStencilAttachment depthStencilAttachment;
    depthStencilAttachment.view = m_depthTextureView;
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.depthLoadOp = LoadOp::Clear;
    depthStencilAttachment.depthStoreOp = StoreOp::Store;
    depthStencilAttachment.depthReadOnly = false;
    depthStencilAttachment.stencilClearValue = 0;
#ifdef WEBGPU_BACKEND_WGPU
    depthStencilAttachment.stencilLoadOp = LoadOp::Clear;
    depthStencilAttachment.stencilStoreOp = StoreOp::Store;
#else
    depthStencilAttachment.stencilLoadOp = LoadOp::Undefined;
    depthStencilAttachment.stencilStoreOp = StoreOp::Undefined;
#endif
    depthStencilAttachment.stencilReadOnly = true;

    renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

    renderPassDesc.timestampWrites = nullptr;

    RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);

    renderPass.setPipeline(m_pipeline);

    renderPass.setVertexBuffer(0, m_vertexBuffer, 0, m_vertexCount * sizeof(VertexAttributes));
    renderPass.setBindGroup(0, m_bindGroup, 0, nullptr);

    renderPass.draw(m_vertexCount, 1, 0, 0);

    renderPass.end();
    renderPass.release();

    CommandBufferDescriptor cmdBufferDescriptor = {};
    cmdBufferDescriptor.label = "Command buffer";
    CommandBuffer command = encoder.finish(cmdBufferDescriptor);
    encoder.release();

    m_queue.submit(command);
    command.release();

#ifndef __EMSCRIPTEN__
    m_surface.present();
#endif

#if defined(WEBGPU_BACKEND_DAWN)
    m_device.tick();
#endif
}

void Application::onResize() {
    // Terminate in reverse order
    terminateDepthBuffer();
    // Re-initialize
    initSurfaceConfiguration();
    initDepthBuffer();

    updateProjectionMatrix();
}

void Application::onFinish() {
    terminateBindGroup();
    terminateUniforms();
    terminateGeometry();
    terminateTexture();
    terminateRenderPipeline();
    terminateDepthBuffer();
    terminateWindowAndDevice();
}

bool Application::isRunning() {
    return !glfwWindowShouldClose(m_window);
}

bool Application::initWindowAndDevice() {
    m_instance = createInstance(InstanceDescriptor{});
    if (!m_instance) {
        cerr << "Could not initialize WebGPU" << endl;
        return false;
    }

    if (!glfwInit()) {
        cerr << "Could not initialize GLFW" << endl;
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);
    if (!m_window) {
        cerr << "Could not create window" << endl;
        return false;
    }

    // Window callbacks
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int, int) {
        auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (that != nullptr) that->onResize();
                                   });
    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xPos, double yPos) {
        auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (that != nullptr) that->onMouseMove(xPos, yPos);
                             });
    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
        auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (that != nullptr) that->onMouseButton(button, action, mods);
                               });
    glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xOffset, double yOffset) {
        auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (that != nullptr) that->onScroll(xOffset, yOffset);
                          });


    cout << "Requesting adapter..." << endl;
    m_surface = glfwGetWGPUSurface(m_instance, m_window);
    RequestAdapterOptions adapterOpts = {};
    adapterOpts.compatibleSurface = m_surface;
    Adapter adapter = m_instance.requestAdapter(adapterOpts);
    cout << "Got adapter: " << adapter << endl;

    SupportedLimits supportedLimits;
    adapter.getLimits(&supportedLimits);

    cout << "Requesting m_device..." << endl;
    RequiredLimits requiredLimits = Default;
    requiredLimits.limits.maxVertexAttributes = 4; // position, normal, color, uv
    requiredLimits.limits.maxVertexBuffers = 1;
    requiredLimits.limits.maxBufferSize = 500000 * sizeof(VertexAttributes);
    requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
    requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
    requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
    requiredLimits.limits.maxInterStageShaderComponents = 8; // color.rgb + normal.xyz + texelCoords.xy
    requiredLimits.limits.maxBindGroups = 1;
    requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
    requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);
    requiredLimits.limits.maxTextureDimension1D = 480;
    requiredLimits.limits.maxTextureDimension2D = 640;
    requiredLimits.limits.maxTextureArrayLayers = 1;
    requiredLimits.limits.maxSampledTexturesPerShaderStage = 1;
    requiredLimits.limits.maxSamplersPerShaderStage = 1;

    int maxTexDimensions = 2048;
    int monitorCount;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    for (int mi = 0; mi < monitorCount; ++mi) {
        int w_width, w_height;
        glfwGetMonitorWorkarea(monitors[mi], nullptr, nullptr, &w_width, &w_height);
        if (maxTexDimensions < w_width) {
            maxTexDimensions = w_width;
            cout << "Updating max texture dimensions; new max: " << maxTexDimensions << "x" << maxTexDimensions << endl;
        }
    }
    requiredLimits.limits.maxTextureDimension1D = maxTexDimensions;
    requiredLimits.limits.maxTextureDimension2D = maxTexDimensions;

    DeviceDescriptor m_deviceDesc = {};
    m_deviceDesc.label = "WGPU Device";
    m_deviceDesc.requiredFeatureCount = 0;
    m_deviceDesc.requiredLimits = nullptr;
    m_deviceDesc.defaultQueue.nextInChain = nullptr;
    m_deviceDesc.defaultQueue.label = "Default queue";
    m_deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message, void* /* pUserData */)
        {
            cout << "Device lost; reason: " << reason;
            if (message)
                cout << " (" << message << ")";
            cout << endl;
        };
    m_deviceDesc.requiredLimits = &requiredLimits;
    m_device = adapter.requestDevice(m_deviceDesc);
    cout << "Got device: " << m_device << endl;

    m_errorCallbackHandle = m_device.setUncapturedErrorCallback([](ErrorType type, char const* message)
                                                                {
                                                                    cout << "Uncaptured Error; type: " << type;
                                                                    if (message) cout << " (" << message << ")";
                                                                    cout << endl; });

    m_queue = m_device.getQueue();

    m_surfaceFormat = m_surface.getPreferredFormat(adapter);

    adapter.release();
    return m_device != nullptr;
}

void Application::terminateWindowAndDevice() {
    m_queue.release();
    m_device.release();
    m_surface.release();
    m_instance.release();

    glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool Application::initSurfaceConfiguration() {

    // Get the current size of the window's framebuffer
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    SurfaceConfiguration surfaceConfig = {};

    surfaceConfig.width = static_cast<uint32_t>(width);
    surfaceConfig.height = static_cast<uint32_t>(height);
    surfaceConfig.usage = TextureUsage::RenderAttachment;
    surfaceConfig.format = m_surfaceFormat;
    surfaceConfig.viewFormatCount = 0;
    surfaceConfig.viewFormats = nullptr;
    surfaceConfig.device = m_device;
    surfaceConfig.presentMode = PresentMode::Fifo;
    surfaceConfig.alphaMode = CompositeAlphaMode::Auto;

    m_surface.configure(surfaceConfig);

    return m_surface != nullptr;
}

bool Application::initDepthBuffer() {

    // Get the current size of the window's framebuffer
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    TextureDescriptor depthTextureDesc;
    depthTextureDesc.dimension = TextureDimension::_2D;
    depthTextureDesc.format = m_depthTextureFormat;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = 1;
    depthTextureDesc.size = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
    depthTextureDesc.usage = TextureUsage::RenderAttachment;
    depthTextureDesc.viewFormatCount = 1;
    depthTextureDesc.viewFormats = (WGPUTextureFormat*)&m_depthTextureFormat;
    m_depthTexture = m_device.createTexture(depthTextureDesc);

    TextureViewDescriptor depthTextureViewDesc;
    depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
    depthTextureViewDesc.baseArrayLayer = 0;
    depthTextureViewDesc.arrayLayerCount = 1;
    depthTextureViewDesc.mipLevelCount = 1;
    depthTextureViewDesc.dimension = TextureViewDimension::_2D;
    depthTextureViewDesc.format = m_depthTextureFormat;
    m_depthTextureView = m_depthTexture.createView(depthTextureViewDesc);

    return (m_depthTexture != nullptr && m_depthTextureView != nullptr);
}

void Application::terminateDepthBuffer() {
    m_depthTexture.release();
    m_depthTextureView.release();
}

bool Application::initRenderPipeline() {

    cout << "Creating shader module..." << endl;
    m_shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/shader.wgsl", m_device);
    cout << "Shader module: " << m_shaderModule << endl;

    RenderPipelineDescriptor pipelineDesc;
    pipelineDesc.label = "Render Pipeline";

    VertexBufferLayout vertexBufferLayout;
    vector<VertexAttribute> vertexAttribs(4); // position, normal, color, uv

    vertexAttribs[0].shaderLocation = 0;
    vertexAttribs[0].format = VertexFormat::Float32x3; //  xyz
    vertexAttribs[0].offset = offsetof(VertexAttributes, position);

    vertexAttribs[1].shaderLocation = 1;
    vertexAttribs[1].format = VertexFormat::Float32x3;
    vertexAttribs[1].offset = offsetof(VertexAttributes, normal);

    vertexAttribs[2].shaderLocation = 2;
    vertexAttribs[2].format = VertexFormat::Float32x3;
    vertexAttribs[2].offset = offsetof(VertexAttributes, color);

    vertexAttribs[3].shaderLocation = 3;
    vertexAttribs[3].format = VertexFormat::Float32x2;
    vertexAttribs[3].offset = offsetof(VertexAttributes, uv);

    vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
    vertexBufferLayout.attributes = vertexAttribs.data();

    vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
    vertexBufferLayout.stepMode = VertexStepMode::Vertex;

    pipelineDesc.vertex.buffers = &vertexBufferLayout;
    pipelineDesc.vertex.bufferCount = 1;

    pipelineDesc.vertex.module = m_shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;

    pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = FrontFace::CCW;
    pipelineDesc.primitive.cullMode = CullMode::None;

    FragmentState fragmentState;
    fragmentState.module = m_shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    BlendState blendState;
    blendState.color.srcFactor = BlendFactor::SrcAlpha;
    blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = BlendOperation::Add;
    blendState.alpha.srcFactor = BlendFactor::Zero;
    blendState.alpha.dstFactor = BlendFactor::One;
    blendState.alpha.operation = BlendOperation::Add;

    ColorTargetState colorTargetState;
    colorTargetState.format = m_surfaceFormat;
    colorTargetState.blend = &blendState;
    colorTargetState.writeMask = ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTargetState;
    pipelineDesc.fragment = &fragmentState;

    DepthStencilState depthStencilState = Default;
    depthStencilState.depthCompare = CompareFunction::Less;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.format = m_depthTextureFormat;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;
    pipelineDesc.depthStencil = &depthStencilState;

    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    vector<BindGroupLayoutEntry> bindingLayoutEntries(3, Default);

    BindGroupLayoutEntry& bindingLayout = bindingLayoutEntries[0];
    bindingLayout.binding = 0;
    bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
    bindingLayout.buffer.type = BufferBindingType::Uniform;
    bindingLayout.buffer.minBindingSize = sizeof(SharedUniforms);

    BindGroupLayoutEntry& textureBindingLayout = bindingLayoutEntries[1];
    textureBindingLayout.binding = 1;
    textureBindingLayout.visibility = ShaderStage::Fragment;
    textureBindingLayout.texture.sampleType = TextureSampleType::Float;
    textureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;

    BindGroupLayoutEntry& samplerBindingLayout = bindingLayoutEntries[2];
    samplerBindingLayout.binding = 2;
    samplerBindingLayout.visibility = ShaderStage::Fragment;
    samplerBindingLayout.sampler.type = SamplerBindingType::Filtering;

    BindGroupLayoutDescriptor bindGroupLayoutDesc;
    bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();
    bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
    m_bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);

    PipelineLayoutDescriptor layoutDesc;
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_bindGroupLayout;
    PipelineLayout layout = m_device.createPipelineLayout(layoutDesc);
    pipelineDesc.layout = layout;

    m_pipeline = m_device.createRenderPipeline(pipelineDesc);
    cout << "Render pipeline: " << m_pipeline << endl;

    return m_pipeline != nullptr;
}

void Application::terminateRenderPipeline() {
    m_pipeline.release();
    m_shaderModule.release();
    m_bindGroupLayout.release();
}

bool Application::initTexture() {
    SamplerDescriptor samplerDesc;
    samplerDesc.addressModeU = AddressMode::Repeat;
    samplerDesc.addressModeV = AddressMode::Repeat;
    samplerDesc.addressModeW = AddressMode::Repeat;
    samplerDesc.magFilter = FilterMode::Linear;
    samplerDesc.minFilter = FilterMode::Linear;
    samplerDesc.mipmapFilter = MipmapFilterMode::Linear;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 8.0f;
    samplerDesc.compare = CompareFunction::Undefined;
    samplerDesc.maxAnisotropy = 1;
    m_sampler = m_device.createSampler(samplerDesc);

    m_texture = ResourceManager::loadTexture(RESOURCE_DIR "/fourareen2k_albedo.jpg", m_device, &m_textureView);
    if (!m_texture) {
        cerr << "Could not load texture!" << endl;
    }

    std::cout << "Texture: " << m_texture << std::endl;
    std::cout << "Texture view: " << m_textureView << std::endl;

    return m_sampler != nullptr;
}

void Application::terminateTexture() {
    m_textureView.release();
    m_texture.destroy();
    m_texture.release();
    m_sampler.release();
}

bool Application::initGeometry() {
    vector<VertexAttributes> vertexData;
    bool success = ResourceManager::loadGeometryFromObj(RESOURCE_DIR "/fourareen.obj", vertexData);
    if (!success) {
        std::cerr << "Could not load geometry!" << endl;
    }

    BufferDescriptor bufferDesc;

    bufferDesc.label = "Vertex Data";
    bufferDesc.size = vertexData.size() * sizeof(VertexAttributes);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
    bufferDesc.mappedAtCreation = false;
    m_vertexBuffer = m_device.createBuffer(bufferDesc);
    m_queue.writeBuffer(m_vertexBuffer, 0, vertexData.data(), bufferDesc.size);

    m_vertexCount = static_cast<int>(vertexData.size());

    return m_vertexBuffer != nullptr;
}

void Application::terminateGeometry() {
    m_vertexBuffer.destroy();
    m_vertexBuffer.release();
    m_vertexCount = 0;
}

bool Application::initUniforms() {
    BufferDescriptor bufferDesc;
    bufferDesc.label = "Uniform Buffer";
    bufferDesc.size = sizeof(SharedUniforms);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;
    m_uniformBuffer = m_device.createBuffer(bufferDesc);

    m_uniforms.modelMatrix = mat4x4(1.0);
    m_uniforms.viewMatrix = glm::lookAt(vec3(-2.0f, -3.0f, 2.0f), vec3(0.0f), vec3(0, 0, 1));
    m_uniforms.projectionMatrix = glm::perspective(45 * PI / 180, 640.0f / 480.0f, 0.01f, 100.0f);
    m_uniforms.time = 1.0f;
    m_uniforms.color = { 0.0f, 1.0f, 0.4f, 1.0f };
    m_queue.writeBuffer(m_uniformBuffer, 0, &m_uniforms, sizeof(SharedUniforms));

    updateViewMatrix();

    return m_uniformBuffer != nullptr;
}

void Application::terminateUniforms() {
    m_uniformBuffer.destroy();
    m_uniformBuffer.release();
}

bool Application::initBindGroup() {
    vector<BindGroupEntry> bindings(3);

    bindings[0].binding = 0;
    bindings[0].buffer = m_uniformBuffer;
    bindings[0].offset = 0;
    bindings[0].size = sizeof(SharedUniforms);

    bindings[1].binding = 1;
    bindings[1].textureView = m_textureView;

    bindings[2].binding = 2;
    bindings[2].sampler = m_sampler;

    BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.layout = m_bindGroupLayout;
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    m_bindGroup = m_device.createBindGroup(bindGroupDesc);

    return m_bindGroup != nullptr;
}

void Application::terminateBindGroup() {
    m_bindGroup.release();
}

void Application::updateProjectionMatrix() {

    // Get the current size of the window's framebuffer
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);
    float ratio = width / (float)height;
    m_uniforms.projectionMatrix = glm::perspective(45 * PI / 180, ratio, 0.01f, 100.0f);
    m_queue.writeBuffer(
        m_uniformBuffer,
        offsetof(SharedUniforms, projectionMatrix),
        &m_uniforms.projectionMatrix,
        sizeof(SharedUniforms::projectionMatrix)
    );
}

void Application::updateViewMatrix() {
    float cx = cos(m_cameraState.angles.x);
    float sx = sin(m_cameraState.angles.y);
    float cy = cos(m_cameraState.angles.x);
    float sy = sin(m_cameraState.angles.y);
    vec3 position = vec3(cx * cy, sx * cy, sy) * std::exp(-m_cameraState.zoom);
    m_uniforms.viewMatrix = glm::lookAt(position, vec3(0.0f), vec3(0, 0, 1));
    m_queue.writeBuffer(
        m_uniformBuffer,
        offsetof(SharedUniforms, viewMatrix),
        &m_uniforms.viewMatrix,
        sizeof(SharedUniforms::viewMatrix)
    );
}

void Application::onMouseMove(double xPos, double yPos) {
    if (m_drag.active) {
        vec2 currentPos = vec2(-(float)xPos, (float)yPos);
        vec2 delta = (currentPos - m_drag.startPos) * m_drag.sensitivity;
        m_cameraState.angles = m_drag.startCameraState.angles + delta;
        // Clamp to avoid going too far when orbiting up/down (y/z plane)
        m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
        updateViewMatrix();
        m_drag.velocity = delta - m_drag.previousDelta;
        m_drag.previousDelta = delta;
    }
}

void Application::onMouseButton(int button, int action, int /* mouse event modifiers */) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        switch (action) {
        case GLFW_PRESS:
            m_drag.active = true;
            double xPos, yPos;
            glfwGetCursorPos(m_window, &xPos, &yPos);
            m_drag.startPos = vec2(-(float)xPos, (float)yPos);
            m_drag.startCameraState = m_cameraState;
            break;
        case GLFW_RELEASE:
            m_drag.active = false;
            break;
        }
    }
}

void Application::onScroll(double /* xOffset */, double yOffset) {
    m_cameraState.zoom += m_drag.scrollSensitivity * static_cast<float>(yOffset);
    m_cameraState.zoom = glm::clamp(m_cameraState.zoom, -2.0f, 2.0f);
}

void Application::updateDragInertia() {
    constexpr float eps = 1e-4f;

    // apply inertia only when the mouse button is released
    if (!m_drag.active) {
        // skip updating the velocity matrix when the velocity is no longer noticeable
        if (std::abs(m_drag.velocity.x) < eps && std::abs(m_drag.velocity.y) < eps) {
            return;
        }
        m_cameraState.angles += m_drag.velocity;
        m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
        // Dampen the velocity by decreasing it exponentially over subsequent frames
        m_drag.velocity *= m_drag.inertia;
        updateViewMatrix();
    }
}

TextureView Application::getNextSurfaceTextureView()
{
    SurfaceTexture surfaceTexture;
    m_surface.getCurrentTexture(&surfaceTexture);

    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success)
    {
        return nullptr;
    }

    Texture texture = surfaceTexture.texture;

    TextureViewDescriptor viewDescriptor;
    viewDescriptor.label = "Surface texture view";
    viewDescriptor.format = wgpuTextureGetFormat(surfaceTexture.texture);
    viewDescriptor.dimension = WGPUTextureViewDimension_2D;
    viewDescriptor.baseMipLevel = 0;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.baseArrayLayer = 0;
    viewDescriptor.arrayLayerCount = 1;
    viewDescriptor.aspect = TextureAspect::All;
    TextureView targetView = texture.createView(viewDescriptor);

    return targetView;
}