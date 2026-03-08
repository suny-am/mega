#pragma once

#include "glfw/include/GLFW/glfw3.h"
#include <cstdint>
#include <memory>
#include <sys/types.h>
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
  wgpu::RequiredLimits _GetRequiredLimits(wgpu::Adapter adapter) const;

private:
  GLFWwindow *window;
  wgpu::Device device;
  wgpu::Queue queue;
  wgpu::Surface surface;
  std::unique_ptr<wgpu::ErrorCallback> uncapturedErrorCallbackHandle;
  wgpu::RenderPipeline pipeline;
  wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
  wgpu::PipelineLayout layout;
  wgpu::BindGroupLayout bindGroupLayout;
  wgpu::BindGroup bindGroup;

  wgpu::Buffer pointBuffer;
  wgpu::Buffer indexBuffer;
  wgpu::Buffer uniformBuffer;
  uint32_t indexCount;
  uint32_t uniformStride;

  struct MyUniforms {
    std::array<float, 4> color;
    float time;
    float _pad[3];
  };
};
