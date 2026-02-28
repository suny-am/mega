// Include WebGPU header
#include <cstddef>
#include <cstdint>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "webgpu-utils.cpp"
#include "webgpu-utils.h"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <cassert>
#include <iostream>

using namespace wgpu;

const char *shaderSource = R"(
    @vertex
    fn vs_main(@location(0) in_vertex_position: vec2f) -> @builtin(position) vec4f {
        return vec4f(in_vertex_position, 0.0, 1.0);
    }

    @fragment
    fn fs_main() -> @location(0) vec4f {
        return vec4f(0.0, 0.4, 1.0, 1.0);
    }
)";

void wgpuPollEvents([[maybe_unused]] Device device,
                    [[maybe_unused]] bool yieldToWebBrowser) {
#if defined(WEBGPU_BACKEND_DAWN)
  device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
  device.poll(false);
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
  if (yieldToWebBrowser) {
    emscripten_sleep(100);
  }
#endif
}

class Application {
public:
  bool Initialize();

  void Terminate();

  void MainLoop();

  bool isRunning();

private:
  TextureView _GetNextSurfaceViewData();
  void _InitializePipeline();
  void _InitializeBuffers();
  RequiredLimits _GetRequiredLimits(Adapter adapter) const;

private:
  GLFWwindow *window;
  Device device;
  Queue queue;
  Surface surface;
  std::unique_ptr<ErrorCallback> uncapturedErrorCallbackHandle;
  RenderPipeline pipeline;
  TextureFormat surfaceFormat = TextureFormat::Undefined;

  Buffer vertexBuffer;
  uint32_t vertexCount;
};

bool Application::Initialize() {
  // NOTE: Open window
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API,
                 GLFW_NO_API); // <-- extra info for glfwCreateWindow
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  window = glfwCreateWindow(640, 480, "Mega", nullptr, nullptr);

  // NOTE: configure instance
  InstanceDescriptor desc = {};

#ifdef WEBGPU_BACKEND_DAWN
  // Make sure the uncaptured error callback is called as soon as an error
  // occurs rather than at the next call to "wgpuDeviceTick".
  WGPUDawnTogglesDescriptor toggles;
  toggles.chain.next = nullptr;
  toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
  toggles.disabledToggleCount = 0;
  toggles.enabledToggleCount = 1;
  const char *toggleName = "enable_immediate_error_handling";
  toggles.enabledToggles = &toggleName;
#endif // WEBGPU_BACKEND_DAWN

  // NOTE: Create instance
#ifdef WEBGPU_BACKEND_EMSCRIPTEN
  WGPUInstance instance = wgpuCreateInstance(nullptr);
#else  //  WEBGPU_BACKEND_EMSCRIPTEN
  Instance instance = wgpuCreateInstance(&desc);
#endif //  WEBGPU_BACKEND_EMSCRIPTEN

  // NOTE: Validation
  if (!instance) {
    std::cerr << "Could not initialize WebGPU!" << std::endl;
    return 1;
  }

  // NOTE: Inspect instance
  std::cout << "WGPU instance: " << instance << std::endl;

  std::cout << "Requesting adapter..." << std::endl;

  // NOTE: Get surface for adapter
  surface = glfwGetWGPUSurface(instance, window);

  // NOTE: configure adapter
  RequestAdapterOptions adapterOpts = {};
  adapterOpts.compatibleSurface = surface;
  Adapter adapter = instance.requestAdapter(adapterOpts);
  std::cout << "Got adapter: " << adapter << std::endl;

  inspectAdapter(adapter);

  // NOTE: Once we have the adapter we no longer need the instance
  instance.release();

  std::cout << "Requesting device..." << std::endl;
  // NOTE: configure device
  RequiredLimits requiredLimits = _GetRequiredLimits(adapter);

  DeviceDescriptor deviceDesc = {};
  deviceDesc.label = "My Device";      // TODO: set to adapter deviceID
  deviceDesc.requiredFeatureCount = 0; // we do not require any specific feature
  deviceDesc.requiredLimits = &requiredLimits;
  deviceDesc.defaultQueue.label = "The default queue";
  // NOTE: Good for debugging
  deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason,
                                     char const *message,
                                     void * /* pUserData */) {
    std::cout << "Device lost: reason " << reason;
    if (message)
      std::cout << " (" << message << ")";
    std::cout << std::endl;
  };

  device = adapter.requestDevice(deviceDesc);

  std::cout << "Got device: " << device << std::endl;

  inspectDevice(device);

  // Good for debugging
  uncapturedErrorCallbackHandle = device.setUncapturedErrorCallback(
      [](ErrorType type, char const *message) {
        std::cout << "Uncaptured device error: type " << type;
        if (message)
          std::cout << " (" << message << ")";
        std::cout << std::endl;
      });

  queue = device.getQueue();

  SurfaceConfiguration surfaceConfig = {};

  surfaceConfig.width = 640;
  surfaceConfig.height = 480;
  surfaceFormat = surface.getPreferredFormat(adapter);
  surfaceConfig.format = surfaceFormat;
  surfaceConfig.viewFormatCount = 0;
  surfaceConfig.viewFormats = nullptr;
  surfaceConfig.usage = TextureUsage::RenderAttachment;
  surfaceConfig.device = device;
  surfaceConfig.presentMode = PresentMode::Fifo;
  surfaceConfig.alphaMode = CompositeAlphaMode::Auto;

  surface.configure(surfaceConfig);
  // NOTE: Once we have the device, we no longer need the adapter
  adapter.release();

  _InitializePipeline();

  _InitializeBuffers();

  return true;
}

void Application::Terminate() {
  // NOTE: Clean up
  vertexBuffer.release();
  pipeline.release();
  surface.unconfigure();
  queue.release();
  surface.release();
  device.release();
  glfwDestroyWindow(window);
  glfwTerminate();
};

void Application::MainLoop() {
  glfwPollEvents();

  // NOTE: Get the target view to present
  TextureView targetView = _GetNextSurfaceViewData();
  if (!targetView)
    return;

  // NOTE: Create the command encoder to write to the draw call
  CommandEncoderDescriptor encoderDesc = {};
  encoderDesc.label = "Debug command encoder";
  CommandEncoder encoder = device.createCommandEncoder(encoderDesc);

  // NOTE: Create the Render Pass descriptor
  RenderPassDescriptor renderPassDesc = {};

  // NOTE: Describe render pass
  RenderPassColorAttachment renderPassColorAttachment = {};
  // NOTE: Describe the attachment
  renderPassDesc.colorAttachmentCount = 1;
  renderPassDesc.colorAttachments = &renderPassColorAttachment;
  renderPassColorAttachment.view = targetView;
  renderPassColorAttachment.resolveTarget = nullptr;
  renderPassColorAttachment.loadOp = LoadOp::Clear;
  renderPassColorAttachment.storeOp = StoreOp::Store;
  renderPassColorAttachment.clearValue = Color{0.9, 0.1, 0.2, 1.0};
  // NOTE: WGPU-Native does not support depth slicing
#ifndef WEBGPU_BACKEND_WGPU
  renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // !WEBGPU_BACKEND_WGPU

  renderPassDesc.depthStencilAttachment = nullptr;
  renderPassDesc.timestampWrites = nullptr;

  // NOTE: Create the render pass encoder
  RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);

  renderPass.setPipeline(pipeline);

  // NOTE: set vertex buffer while encoding render pass
  renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexBuffer.getSize());

  renderPass.draw(vertexCount, 1, 0, 0);

  // NOTE: use render pass
  renderPass.end();
  renderPass.release();

  CommandBufferDescriptor cmdBufferDesc = {};
  cmdBufferDesc.label = "Command buffer";
  CommandBuffer command = encoder.finish(cmdBufferDesc);
  encoder.release();

  std::cout << "Submitting command..." << std::endl;
  queue.submit(1, &command);
  command.release();
  std::cout << "Command submitted" << std::endl;

  targetView.release();
#ifndef __EMSCRIPTEN
  surface.present();
#endif // !__EMSCRIPTEN__
       //
#if defined(WEBGPU_BACKEND_DAWN)
  std::cout << "Waiting for tick..." << std::endl;
  wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
  std::cout << "Polling device..." << std::endl;
  device.poll(false);
#endif
};

bool Application::isRunning() { return !glfwWindowShouldClose(window); }

TextureView Application::_GetNextSurfaceViewData() {

  SurfaceTexture surfaceTexture;
  surface.getCurrentTexture(&surfaceTexture);

  if (surfaceTexture.status != SurfaceGetCurrentTextureStatus::Success) {
    return nullptr;
  }

  TextureViewDescriptor textureViewDesc = {};
  textureViewDesc.label = "Surface texture view";
  textureViewDesc.format = wgpuTextureGetFormat(surfaceTexture.texture);
  textureViewDesc.dimension = WGPUTextureViewDimension_2D;
  textureViewDesc.baseMipLevel = 0;
  textureViewDesc.mipLevelCount = 1;
  textureViewDesc.baseArrayLayer = 0;
  textureViewDesc.arrayLayerCount = 1;
  textureViewDesc.aspect = TextureAspect::All;
  TextureView targetView =
      wgpuTextureCreateView(surfaceTexture.texture, &textureViewDesc);

#ifndef WEBGPU_BACKEND_WGPU
  // NOTE: for WGPU-Native, surface textures have to be released AFTER
  // presentation
  surfaceTexture.texture.release();
#endif // !WEBGPU_BACKEND_WGPU

  return targetView;
}

void Application::_InitializePipeline() {
  // NOTE: Create shader module
  ShaderModuleDescriptor shaderDesc;
#ifdef WEBGPU_BACKEND_WGPU
  shaderDesc.hintCount = 0;
  shaderDesc.hints = nullptr;
#endif

  ShaderModuleWGSLDescriptor shaderCodeDesc;
  shaderCodeDesc.chain.next = nullptr;
  shaderCodeDesc.chain.sType = SType::ShaderModuleWGSLDescriptor;
  shaderDesc.nextInChain = &shaderCodeDesc.chain;

  shaderCodeDesc.code = shaderSource;

  ShaderModule shaderModule = device.createShaderModule(shaderDesc);

  // NOTE: Describe pipeline
  RenderPipelineDescriptor pipelineDesc;

  // NOTE: Describe pipeline

  VertexBufferLayout vertexBufferLayout;
  VertexAttribute positionAttrib;

  positionAttrib.shaderLocation = 0;
  positionAttrib.format = VertexFormat::Float32x2;
  positionAttrib.offset = 0;

  vertexBufferLayout.attributeCount = 1;
  vertexBufferLayout.attributes = &positionAttrib;
  vertexBufferLayout.arrayStride = 2 * sizeof(float);
  vertexBufferLayout.stepMode = VertexStepMode::Vertex;

  pipelineDesc.vertex.bufferCount = 1;
  pipelineDesc.vertex.buffers = &vertexBufferLayout;

  pipelineDesc.vertex.module = shaderModule;
  pipelineDesc.vertex.entryPoint = "vs_main";
  pipelineDesc.vertex.constantCount = 0;
  pipelineDesc.vertex.constants = nullptr;

  pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
  pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
  pipelineDesc.primitive.frontFace = FrontFace::CCW;
  // NOTE: good for develipment; as non-manyfold topology can sometimes be
  // obscured
  pipelineDesc.primitive.cullMode = CullMode::None;

  FragmentState fragmentState;
  fragmentState.module = shaderModule;
  fragmentState.entryPoint = "fs_main";
  fragmentState.constantCount = 0;
  fragmentState.constants = nullptr;

  // NOTE: we do not use stencil/depth testing for now
  pipelineDesc.depthStencil = nullptr;

  BlendState blendState;
  blendState.color.srcFactor = BlendFactor::SrcAlpha;
  blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
  blendState.color.operation = BlendOperation::Add;

  blendState.alpha.srcFactor = BlendFactor::Zero;
  blendState.alpha.dstFactor = BlendFactor::One;
  blendState.alpha.operation = BlendOperation::Add;

  ColorTargetState colorTarget;
  colorTarget.format = surfaceFormat;
  colorTarget.blend = &blendState;
  colorTarget.writeMask = ColorWriteMask::All;

  fragmentState.targetCount = 1;
  fragmentState.targets = &colorTarget;

  pipelineDesc.fragment = &fragmentState;
  // NOTE: sammple per pixel
  pipelineDesc.multisample.count = 1;
  // NOTE: default value for mask, meaning "all bits on"
  pipelineDesc.multisample.mask = ~0u;
  // NOTE: Default value as well (irrelevant for count = 1 anyways)
  pipelineDesc.multisample.alphaToCoverageEnabled = false;
  pipelineDesc.layout = nullptr;

  pipeline = device.createRenderPipeline(pipelineDesc);

  shaderModule.release();
}

void Application::_InitializeBuffers() {

  // NOTE: setup vertex buffer data
  std::vector<float> vertexData = {
      // xy0
      -0.5,
      -0.5,
      // xy1
      +0.5,
      -0.5,
      // xy2
      +0.0,
      +0.5,

      // xy3
      -0.55f,
      -0.5,

      // xy4
      -0.05f,
      +0.5,

      // xy5
      -0.55f,
      +0.5,

  };

  vertexCount = static_cast<uint32_t>(vertexData.size() / 2);

  BufferDescriptor bufferDesc;
  bufferDesc.size = vertexData.size() * sizeof(float);
  bufferDesc.usage =
      BufferUsage::CopyDst | BufferUsage::Vertex; // Vertex usage here!
  bufferDesc.mappedAtCreation = false;
  vertexBuffer = device.createBuffer(bufferDesc);

  queue.writeBuffer(vertexBuffer, 0, vertexData.data(), bufferDesc.size);
}

RequiredLimits Application::_GetRequiredLimits(Adapter adapter) const {
  SupportedLimits supportedLimits;
  adapter.getLimits(&supportedLimits);

  RequiredLimits requiredLimits = Default;

  // NOTE: only one vertex attribute for now
  requiredLimits.limits.maxVertexAttributes = 1;
  // NOTE: only one vertex buffer for now
  requiredLimits.limits.maxVertexBuffers = 1;
  // NOTE: max buffer size is 6 vertices of 2 float each
  requiredLimits.limits.maxBufferSize = 6 * 2 * sizeof(float);
  // NOTE: max stride between 2 consecutive vertices in the vertex buffer
  requiredLimits.limits.maxVertexBufferArrayStride = 2 * sizeof(float);

  // NOTE: safeguard minimum values for buffe offsets
  requiredLimits.limits.minUniformBufferOffsetAlignment =
      supportedLimits.limits.minUniformBufferOffsetAlignment;
  requiredLimits.limits.minStorageBufferOffsetAlignment =
      supportedLimits.limits.minStorageBufferOffsetAlignment;

  // Note forward max texture dimensions allowed
  requiredLimits.limits.maxTextureDimension2D =
      supportedLimits.limits.maxTextureDimension2D;

  return requiredLimits;
}

int main(int, char **) {

  Application app;

  if (!app.Initialize()) {
    return 1;
  }

#ifdef __EMSCRIPTEN
  // Emscripten main loop
  auto callback = [](void *arg)
}
Application *pApp = reinterpret_cast<Application *>(arg);
pApp->MainLoop();
emscripten_set_main_loop_arg(callback, &app, 0, true);
#else
  while (app.isRunning()) {
    app.MainLoop();
  }
#endif //  __EMSCRIPTEN

app.Terminate();

return 0;
}
