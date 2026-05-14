#pragma once

#include "glfw/include/GLFW/glfw3.h"
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <webgpu/webgpu.hpp>

class Application {
public:
  bool Initialize();

  void Terminate();

  void MainLoop();

  bool isRunning();

private:
  wgpu::TextureView _GetNextSurfaceViewData();
  void _InitializePipeline();
  void _InitializeBuffers();
  void _InitializeBindGroups();
  void _InitializeDepthStencil();
  wgpu::RequiredLimits _GetRequiredLimits(wgpu::Adapter adapter) const;

private:
  GLFWwindow *window;
  wgpu::Device device;
  wgpu::Queue queue;
  wgpu::Surface surface;
  std::unique_ptr<wgpu::ErrorCallback> uncapturedErrorCallbackHandle;
  wgpu::RenderPipeline pipeline;
  wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
  wgpu::TextureFormat depthTextureFormat = wgpu::TextureFormat::Undefined;
  wgpu::TextureView depthTextureView;
  wgpu::Texture depthTexture;
  wgpu::PipelineLayout layout;
  wgpu::BindGroupLayout bindGroupLayout;
  wgpu::BindGroup bindGroup;

  wgpu::Buffer pointBuffer;
  wgpu::Buffer indexBuffer;
  wgpu::Buffer uniformBuffer;
  uint32_t indexCount;

  struct MyUniforms {
    glm::mat4x4 projectionMatrix;
    glm::mat4x4 viewMatrix;
    glm::mat4x4 modelMatrix;
    glm::vec4 color;
    float time;
    float _pad[3];
  } uniforms;
};
