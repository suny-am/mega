#include "Application.h"
#include "ResourceManager.h"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "webgpu-utils.h"
#include <GLFW/glfw3.h>
#include <cstddef>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif // __EMSCRIPTEN__

using namespace wgpu;
using namespace glm;

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

  //  InspectAdapter(adapter);

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

  //  InspectDevice(device);

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
  _InitializeTextures();
  _InitializeDepthStencil();
  _InitializeBuffers();
  _InitializeBindGroups();

  return true;
}

void Application::Terminate() {
  // NOTE: Clean up
  vertexBuffer.release();
  uniformBuffer.release();
  colorTexture.destroy();
  colorTexture.release();
  layout.release();
  bindGroupLayout.release();
  bindGroup.release();
  pipeline.release();
  surface.unconfigure();
  depthTextureView.release();
  queue.release();
  surface.release();
  device.release();
  glfwDestroyWindow(window);
  glfwTerminate();
};

void Application::MainLoop() {
  glfwPollEvents();

  // Update uniform buffer
  uniforms.time =
      static_cast<float>(glfwGetTime()); // glfwGetTime returns a double
  // Only update the 1-st float of the buffer
  queue.writeBuffer(uniformBuffer, offsetof(MyUniforms, time), &uniforms.time,
                    sizeof(MyUniforms::time));

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
  renderPassColorAttachment.clearValue = Color{0.05, 0.05, 0.05, 1.0};
  // NOTE: WGPU-Native does not support depth slicing
#ifndef WEBGPU_BACKEND_WGPU
  renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // !WEBGPU_BACKEND_WGPU

  RenderPassDepthStencilAttachment depthStencilAttachment;
  depthStencilAttachment.view = depthTextureView;
  depthStencilAttachment.depthClearValue = 1.0f;
  depthStencilAttachment.depthLoadOp = LoadOp::Clear;
  depthStencilAttachment.depthStoreOp = StoreOp::Store;
  depthStencilAttachment.depthReadOnly = false;
  depthStencilAttachment.stencilClearValue = 0;
  depthStencilAttachment.stencilLoadOp = LoadOp::Clear;
  depthStencilAttachment.stencilStoreOp = StoreOp::Store;
  depthStencilAttachment.stencilReadOnly = true;

#ifdef WEBGPU_BACKEND_DAWN
  depthStencilAttachment.stencilLoadOp = LoadOp::Undefined;
  depthStencilAttachment.stencilStoreOp = StoreOp::Undefined;
  constexpr auto NaNf = std::vnumeric_limits<float>::quiet_NaN();
  depthStencilAttachment.clearDepth = NaNf;
#endif

  renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

  renderPassDesc.timestampWrites = nullptr;

  // NOTE: Create the render pass encoder
  RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);

  renderPass.setPipeline(pipeline);

  // NOTE: set vertex buffer while encoding render pass
  renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexBuffer.getSize());

  // NOTE: set bind group for render pass
  renderPass.setBindGroup(0, bindGroup, 0, nullptr);

  renderPass.draw(indexCount, 1, 0, 0);

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

TextureView Application::_GetNextSurfaceViewData() {

  SurfaceTexture surfaceTexture;
  surface.getCurrentTexture(&surfaceTexture);

  if (surfaceTexture.status != SurfaceGetCurrentTextureStatus::Success) {
    return nullptr;
  }

  Texture texture = surfaceTexture.texture;

  TextureViewDescriptor textureViewDesc = {};
  textureViewDesc.label = "Surface texture view";
  textureViewDesc.format = wgpuTextureGetFormat(surfaceTexture.texture);
  textureViewDesc.dimension = WGPUTextureViewDimension_2D;
  textureViewDesc.baseMipLevel = 0;
  textureViewDesc.mipLevelCount = 1;
  textureViewDesc.baseArrayLayer = 0;
  textureViewDesc.arrayLayerCount = 1;
  textureViewDesc.aspect = TextureAspect::All;
  TextureView targetView = texture.createView(textureViewDesc);

#ifndef WEBGPU_BACKEND_WGPU
  // NOTE: for WGPU-Native, surface textures have to be released AFTER
  // presentation
  surfaceTexture.texture.release();
#endif // !WEBGPU_BACKEND_WGPU

  return targetView;
}

void Application::_InitializeTextures() {
  // NOTE: we do not have any textures in this sample, but this is where you
  // would initialize them
  TextureDescriptor textureDesc;
  // [...] setup texture descriptor
  textureDesc.dimension = TextureDimension::_2D;
  textureDesc.size = {256, 256, 1};
  textureDesc.mipLevelCount = 1;
  textureDesc.sampleCount = 1;
  textureDesc.format = TextureFormat::RGBA8Unorm;
  textureDesc.usage = TextureUsage::TextureBinding | TextureUsage::CopyDst;
  textureDesc.viewFormatCount = 0;
  textureDesc.viewFormats = nullptr;

  colorTexture = device.createTexture(textureDesc);
  std::cout << "Color texture: " << colorTexture << std::endl;

  std::vector<uint8> pixels(4 * textureDesc.size.width *
                            textureDesc.size.height);
  for (uint32_t i = 0; i < textureDesc.size.width; ++i) {
    for (uint32_t j = 0; j < textureDesc.size.height; ++j) {
      uint8_t *p = &pixels[4 * (j * textureDesc.size.width + i)];
      p[0] = (i / 16) % 2 == (j / 16) % 2 ? 255 : 0; // R
      p[1] = ((i - j) / 16) % 2 == 0 ? 255 : 0;      // G
      p[2] = ((i + j) / 16) % 2 == 0 ? 255 : 0;      // B
      p[3] = 255;                                    // A
    }
  }

  TextureViewDescriptor textureViewDesc;
  textureViewDesc.aspect = TextureAspect::All;
  textureViewDesc.baseArrayLayer = 0;
  textureViewDesc.arrayLayerCount = 1;
  textureViewDesc.baseMipLevel = 0;
  textureViewDesc.mipLevelCount = 1;
  textureViewDesc.dimension = TextureViewDimension::_2D;
  textureViewDesc.format = textureDesc.format;
  colorTextureView = colorTexture.createView(textureViewDesc);
  std::cout << "Color texture view: " << colorTextureView << std::endl;

  ImageCopyTexture destination;
  destination.texture = colorTexture;
  destination.mipLevel = 0;
  destination.origin = {0, 0, 0};
  destination.aspect = TextureAspect::All;

  TextureDataLayout source;
  source.offset = 0;
  source.bytesPerRow = 4 * textureDesc.size.width;
  source.rowsPerImage = textureDesc.size.height;

  queue.writeTexture(destination, pixels.data(), pixels.size(), source,
                     textureDesc.size);
}

void Application::_InitializeDepthStencil() {
  TextureDescriptor depthTextureDesc;
  depthTextureDesc.dimension = TextureDimension::_2D;
  depthTextureDesc.format = depthTextureFormat;
  depthTextureDesc.mipLevelCount = 1;
  depthTextureDesc.sampleCount = 1;
  depthTextureDesc.size = {640, 480, 1};
  depthTextureDesc.usage = TextureUsage::RenderAttachment;
  depthTextureDesc.viewFormatCount = 1;
  depthTextureDesc.viewFormats = (WGPUTextureFormat *)&depthTextureFormat;
  depthTexture = device.createTexture(depthTextureDesc);
  std::cout << "Depth texture: " << depthTexture << std::endl;

  TextureViewDescriptor depthTextureViewDesc;
  depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
  depthTextureViewDesc.baseArrayLayer = 0;
  depthTextureViewDesc.arrayLayerCount = 1;
  depthTextureViewDesc.baseMipLevel = 0;
  depthTextureViewDesc.mipLevelCount = 1;
  depthTextureViewDesc.dimension = TextureViewDimension::_2D;
  depthTextureViewDesc.format = depthTextureFormat;
  depthTextureView = depthTexture.createView(depthTextureViewDesc);
  std::cout << "Depth texture view: " << depthTextureView << std::endl;
}

void Application::_InitializePipeline() {

  static_assert(sizeof(MyUniforms) % 16 == 0,
                "Uniform buffer struct size must be a multiple of 16 bytes");

  // NOTE: Create shader module

  std::cout << "Creating shader module..." << std::endl;
  ShaderModule shaderModule =
      ResourceManager::LoadShaderModule(RESOURCE_DIR "/shader.wgsl", device);
  std::cout << "Shader module: " << shaderModule << std::endl;

  // Check for errors
  if (shaderModule == nullptr) {
    std::cerr << "Could not load shader!" << std::endl;
    exit(1);
  }

  // NOTE: Describe pipeline
  RenderPipelineDescriptor pipelineDesc;

  // NOTE: Describe pipeline
  VertexBufferLayout vertexBufferLayout;

  // NOTE: 3 attributes: position, normal and color
  std::vector<VertexAttribute> vertexAttribs(3);

  // NOTE: describe position attribute
  vertexAttribs[0].shaderLocation = 0;
  vertexAttribs[0].format = VertexFormat::Float32x3;
  vertexAttribs[0].offset = offsetof(VertexAttributes, position);

  // NOTE: describe normal attribute
  vertexAttribs[1].shaderLocation = 1;
  vertexAttribs[1].format = VertexFormat::Float32x3;
  vertexAttribs[1].offset = offsetof(VertexAttributes, normal);

  // NOTE: describe color attribute
  vertexAttribs[2].shaderLocation = 2;
  vertexAttribs[2].format = VertexFormat::Float32x3;
  vertexAttribs[2].offset = offsetof(VertexAttributes, color);

  vertexBufferLayout.attributeCount =
      static_cast<uint32_t>(vertexAttribs.size());
  vertexBufferLayout.attributes = vertexAttribs.data();
  vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
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

  DepthStencilState depthStencilState = Default;
  depthStencilState.depthCompare = CompareFunction::Less;
  // NOTE: it is good to disable the depth writing for transparent objects and
  // UI elements
  depthStencilState.depthWriteEnabled = true;
  depthTextureFormat = TextureFormat::Depth24Plus;
  depthStencilState.format = depthTextureFormat;
  // NOTE: deactivate stencil
  depthStencilState.stencilReadMask = 0;
  depthStencilState.stencilWriteMask = 0;

  pipelineDesc.depthStencil = &depthStencilState;

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

  // Since we now have 2 bindings, we use a vector to store them
  std::vector<BindGroupLayoutEntry> bindingLayoutEntries(2, Default);

  // The uniform buffer binding that we already had
  BindGroupLayoutEntry &bindingLayout = bindingLayoutEntries[0];
  bindingLayout.binding = 0;
  bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
  bindingLayout.buffer.type = BufferBindingType::Uniform;
  bindingLayout.buffer.minBindingSize = sizeof(MyUniforms);

  // The texture binding
  BindGroupLayoutEntry &textureBindingLayout = bindingLayoutEntries[1];
  textureBindingLayout.binding = 1;
  textureBindingLayout.visibility = ShaderStage::Fragment;
  textureBindingLayout.texture.sampleType = TextureSampleType::Float;
  textureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;

  // Create a bind group layout
  BindGroupLayoutDescriptor bindGroupLayoutDesc{};
  bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();
  bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
  bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutDesc);

  PipelineLayoutDescriptor layoutDesc{};
  layoutDesc.bindGroupLayoutCount = 1;
  layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout *)&bindGroupLayout;
  layout = device.createPipelineLayout(layoutDesc);

  pipelineDesc.layout = layout;

  pipeline = device.createRenderPipeline(pipelineDesc);

  shaderModule.release();
}

void Application::_InitializeBindGroups() {
  std::vector<BindGroupEntry> bindings(2);

  bindings[0].binding = 0;
  bindings[0].buffer = uniformBuffer;
  bindings[0].offset = 0;
  bindings[0].size = sizeof(MyUniforms);

  bindings[1].binding = 1;
  bindings[1].textureView = colorTextureView;

  BindGroupDescriptor bindGroupDesc;
  bindGroupDesc.layout = bindGroupLayout;
  bindGroupDesc.entryCount = (uint32_t)bindings.size();
  bindGroupDesc.entries = bindings.data();
  bindGroup = device.createBindGroup(bindGroupDesc);
}

void Application::_InitializeBuffers() {
  // NOTE: setup vertex buffer data
  std::vector<float> pointData;
  std::vector<VertexAttributes> vertexData;

  bool success = ResourceManager::LoadGeometryFromObj(RESOURCE_DIR "/plane.obj",
                                                      vertexData);

  if (!success) {
    std::cerr << "Could not load geometry" << std::endl;
    exit(1);
  }

  indexCount = static_cast<int>(vertexData.size());

  // NOTE: common buffer config
  BufferDescriptor bufferDesc;
  bufferDesc.mappedAtCreation = false;

  // NOTE: position buffer
  bufferDesc.label = "Vertex buffer";
  bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
  bufferDesc.size = vertexData.size() * sizeof(VertexAttributes);
  vertexBuffer = device.createBuffer(bufferDesc);

  queue.writeBuffer(vertexBuffer, 0, vertexData.data(), bufferDesc.size);

  // NOTE: uniform buffer
  bufferDesc.label = "Vertex uniforms";
  bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
  bufferDesc.size = sizeof(MyUniforms);
  uniformBuffer = device.createBuffer(bufferDesc);

  // NOTE: update uniforms here
  uniforms.modelMatrix = mat4x4(1.0);
  uniforms.viewMatrix = scale(mat4x4(1.0), vec3(1.0f));
  uniforms.projectionMatrix = ortho(-1, 1, -1, 1, -1, 1);
  uniforms.time = 1.0f;
  uniforms.color = {0.0f, 1.0f, 0.4f, 1.0f};
  queue.writeBuffer(uniformBuffer, 0, &uniforms, sizeof(MyUniforms));
}

RequiredLimits Application::_GetRequiredLimits(Adapter adapter) const {
  SupportedLimits supportedLimits;
  adapter.getLimits(&supportedLimits);
  RequiredLimits requiredLimits = Default;
  requiredLimits.limits = supportedLimits.limits;
  requiredLimits.limits.maxVertexAttributes = 3;
  requiredLimits.limits.maxVertexBuffers = 1;
  requiredLimits.limits.maxBufferSize = 10000 * sizeof(VertexAttributes);
  requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
  requiredLimits.limits.minStorageBufferOffsetAlignment =
      supportedLimits.limits.minStorageBufferOffsetAlignment;
  requiredLimits.limits.minUniformBufferOffsetAlignment =
      supportedLimits.limits.minUniformBufferOffsetAlignment;
  requiredLimits.limits.maxInterStageShaderComponents = 6;
  requiredLimits.limits.maxBindGroups = 1;
  requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
  requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);
  requiredLimits.limits.maxTextureDimension1D = 480;
  requiredLimits.limits.maxTextureDimension2D = 640;
  requiredLimits.limits.maxTextureArrayLayers = 1;
  // Add the possibility to sample a texture in a shader
  requiredLimits.limits.maxSampledTexturesPerShaderStage = 1;

  return requiredLimits;
}

bool Application::isRunning() { return !glfwWindowShouldClose(window); }
