#pragma once

#include <glm/glm.hpp>
#include <webgpu/webgpu.hpp>

// Forward declare
struct GLFWwindow;

class Application {
public:
  enum class CameraMode { Arcball, FirstPerson, Trackpad };
  enum class MouseMode { Mouse, Trackpad };

public:
  // A function called only once at the beginning. Returns false is init failed.
  bool onInit();

  // A function called at each frame, guaranteed never to be called before
  // `onInit`.
  void onFrame();

  // A function called when the window is resized, with the new width and
  // height.
  void onResize();

  void onMouseMove(double xPos, double yPos);
  void onMouseButton(int button, int action, int mods);
  void onScroll(double xOffset, double yOffset);

  void updateProjectionMatrix();

  void updateViewMatrix();

  // A function called only once at the very end.
  void onFinish();

  // A function that tells if the application is still running.
  bool isRunning();

  void setMouseMode(MouseMode mode);

private:
  bool initWindowAndDevice();
  void terminateWindowAndDevice();

  bool initDepthBuffer();
  void terminateDepthBuffer();

  bool initSurfaceConfiguration();

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

  void updateDragInertia();

  wgpu::TextureView getNextSurfaceViewData();

private:
  // (Just aliases to make notations lighter)
  using mat4x4 = glm::mat4x4;
  using vec4 = glm::vec4;
  using vec3 = glm::vec3;
  using vec2 = glm::vec2;

  /**
   * The same structure as in the shader, replicated in C++
   */
  struct MyUniforms {
    // We add transform matrices
    mat4x4 projectionMatrix;
    mat4x4 viewMatrix;
    mat4x4 modelMatrix;
    vec4 color;
    float time;
    float _pad[3];
  };
  // Have the compiler check byte alignment
  static_assert(sizeof(MyUniforms) % 16 == 0);

  struct CameraState {
    // TODO: change camera logic based on mode
    CameraMode mode = CameraMode::Arcball;
    vec2 angles = {0.8f, 0.5f};
    float zoom = -1.2f;
  };

  struct DragState {
    bool active = false;

    vec2 startMouse;

    CameraState startCameraState;

    float sensitivity = 0.01f;
    float scrollSensitivity = 0.1f;

    // inertia
    vec2 velocity = {0.0f, 0.0f};
    vec2 previousDelta;
    float inertia = 0.9f;
  };

  // Window and Device
  GLFWwindow *m_window = nullptr;
  wgpu::Instance m_instance = nullptr;
  wgpu::Surface m_surface = nullptr;
  wgpu::TextureFormat m_surfaceFormat = wgpu::TextureFormat::Undefined;
  wgpu::Device m_device = nullptr;
  wgpu::Queue m_queue = nullptr;
  // Keep the error callback alive
  std::unique_ptr<wgpu::ErrorCallback> m_errorCallbackHandle;

  // Depth Buffer
  wgpu::TextureFormat m_depthTextureFormat = wgpu::TextureFormat::Depth24Plus;
  wgpu::Texture m_depthTexture = nullptr;
  wgpu::TextureView m_depthTextureView = nullptr;

  // Render Pipeline
  wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
  wgpu::ShaderModule m_shaderModule = nullptr;
  wgpu::RenderPipeline m_pipeline = nullptr;

  // Texture
  wgpu::Sampler m_sampler = nullptr;
  wgpu::Texture m_texture = nullptr;
  wgpu::TextureView m_textureView = nullptr;

  // Geometry
  wgpu::Buffer m_vertexBuffer = nullptr;
  int m_vertexCount = 0;

  // Uniforms
  wgpu::Buffer m_uniformBuffer = nullptr;
  MyUniforms m_uniforms;

  // Bind Group
  wgpu::BindGroup m_bindGroup = nullptr;

  // Camera state
  CameraState m_cameraState;

  // Drag state
  DragState m_drag;
};
