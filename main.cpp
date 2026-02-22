#include "webgpu-utils.cpp"
#include "webgpu-utils.h"

// Include WebGPU header
#include <cassert>
#include <webgpu/webgpu.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif // __EMSCRIPTEN__
#ifdef WEBGPU_BACKEND_WGPU
#include <webgpu/wgpu.h>
#endif // WEBGPU_BACKEND_WGPU
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <iostream>

class Application {
public:
  bool Initialize();

  void Terminate();

  void MainLoop();

  bool isRunning();

  GLFWwindow *window;
  WGPUDevice device;
  WGPUQueue queue;
  WGPUSurface surface;

private:
};

bool Application::Initialize() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API,
                 GLFW_NO_API); // <-- extra info for glfwCreateWindow
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);

  // We create a descriptor
  WGPUInstanceDescriptor desc = {};
  desc.nextInChain = nullptr;

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

  desc.nextInChain = &toggles.chain;
#endif // WEBGPU_BACKEND_DAWN

  // Create instance
#ifdef WEBGPU_BACKEND_EMSCRIPTEN
  WGPUInstance instance = wgpuCreateInstance(nullptr);
#else  //  WEBGPU_BACKEND_EMSCRIPTEN
  WGPUInstance instance = wgpuCreateInstance(&desc);
#endif //  WEBGPU_BACKEND_EMSCRIPTEN

  // Validation
  if (!instance) {
    std::cerr << "Could not initialize WebGPU!" << std::endl;
    return 1;
  }

  // Inspect instance
  std::cout << "WGPU instance: " << instance << std::endl;

  std::cout << "Requesting adapter..." << std::endl;

  // get surface for adapter
  surface = glfwGetWGPUSurface(instance, window);

  WGPURequestAdapterOptions adapterOpts = {};
  adapterOpts.nextInChain = nullptr;
  adapterOpts.compatibleSurface = surface;
  WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);
  // Once we have the adapter we no longer need the instance
  wgpuInstanceRelease(instance);

  std::cout << "Got adapter: " << adapter << std::endl;

  inspectAdapter(adapter);

  std::cout << "Requesting device..." << std::endl;

  WGPUDeviceDescriptor deviceDesc = {};
  deviceDesc.nextInChain = nullptr;
  deviceDesc.label = "My Device";      // TODO: set to adapter deviceID
  deviceDesc.requiredFeatureCount = 0; // we do not require any specific feature
  deviceDesc.requiredLimits = nullptr; // we do not require any specific limit
  deviceDesc.defaultQueue.nextInChain = nullptr;
  deviceDesc.defaultQueue.label = "The default queue";
  // Good for debugging
  deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason,
                                     char const *message,
                                     void * /* pUserData */) {
    std::cout << "Device lost: reason " << reason;
    if (message)
      std::cout << " (" << message << ")";
    std::cout << std::endl;
  };
  device = requestDeviceSync(adapter, &deviceDesc);

  std::cout << "Got device: " << device << std::endl;

  // Once we have the device, we no longer need the adapter
  wgpuAdapterRelease(adapter);

  inspectDevice(device);

  // Good for debugging
  auto onDeviceError = [](WGPUErrorType type, char const *message,
                          void * /* pUserData */) {
    std::cout << "Uncaptured device error: type " << type;
    if (message)
      std::cout << " (" << message << ")";
    std::cout << std::endl;
  };
  wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError,
                                       nullptr /* pUserData */);

  queue = wgpuDeviceGetQueue(device);

  auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status,
                            void * /* pUserData */) {
    std::cout << "Queued work finished with status: " << status << std::endl;
  };
  wgpuQueueOnSubmittedWorkDone(queue, onQueueWorkDone, nullptr /* pUserData */);

  WGPUCommandEncoderDescriptor encoderDesc = {};
  encoderDesc.nextInChain = nullptr;
  encoderDesc.label = "Debug command encoder";
  WGPUCommandEncoder encoder =
      wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

  wgpuCommandEncoderInsertDebugMarker(encoder, "First command");
  wgpuCommandEncoderInsertDebugMarker(encoder, "Second command");

  WGPUCommandBufferDescriptor cmdBufferDesc = {};
  cmdBufferDesc.nextInChain = nullptr;
  cmdBufferDesc.label = "Command buffer";
  WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);

  std::cout << "Submitting command..." << std::endl;
  wgpuQueueSubmit(queue, 1, &command);
  wgpuCommandBufferRelease(command);
  std::cout << "Command submitted" << std::endl;
  return true;
}

void Application::Terminate() {
  // Clean up
  wgpuDeviceRelease(device);
  wgpuQueueRelease(queue);
  wgpuSurfaceRelease(surface);
  glfwDestroyWindow(window);
  glfwTerminate();
};

void Application::MainLoop() {
  glfwPollEvents();

#if defined(WEBGPU_BACKEND_DAWN)
  std::cout << "Waiting for tick..." << std::endl;
  wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
  std::cout << "Polling device..." << std::endl;
  wgpuDevicePoll(device, false, nullptr);
#endif
};

bool Application::isRunning() { return !glfwWindowShouldClose(window); }

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
