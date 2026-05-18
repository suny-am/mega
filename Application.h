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

public:
  struct VertexAttributes {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;
  };

private:
  wgpu::TextureView _GetNextSurfaceViewData();
  void _InitializePipeline();
  void _InitializeBuffers();
  void _InitializeBindGroups();
  void _InitializeDepthStencil();
  void _InitializeTextures();
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
  wgpu::TextureView colorTextureView;
  wgpu::Texture depthTexture;
  wgpu::Texture colorTexture;
  wgpu::PipelineLayout layout;
  wgpu::BindGroupLayout bindGroupLayout;
  wgpu::BindGroup bindGroup;
  wgpu::Sampler sampler;

  wgpu::Buffer vertexBuffer;
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
