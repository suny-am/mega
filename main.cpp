// Include WebGPU header
#include "build/_deps/webgpu-backend-wgpu-src/include/webgpu/webgpu.h"
#include <cassert>
#include <cstddef>
#include <webgpu/webgpu.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif // __EMSCRIPTEN__
#ifdef WEBGPU_BACKEND_WGPU
#include <webgpu/wgpu.h>
#endif // WEBGPU_BACKEND_WGPU
#include <iostream>

WGPUDevice requestDeviceSync(WGPUAdapter adapter,
                             WGPUDeviceDescriptor const *descriptor) {
  struct UserData {
    WGPUDevice device = nullptr;
    bool requestEnded = false;
  };
  UserData userData;

  auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status,
                                 WGPUDevice device, char const *message,
                                 void *pUserData) {
    UserData &userData = *reinterpret_cast<UserData *>(pUserData);
    if (status == WGPURequestDeviceStatus_Success) {
      userData.device = device;
    } else {
      std::cout << "Could not get WebGPU device: " << message << std::endl;
    }
    userData.requestEnded = true;
  };

  wgpuAdapterRequestDevice(adapter, descriptor, onDeviceRequestEnded,
                           (void *)&userData);

#ifdef __EMSCRIPTEN__
  while (!userData.requestEnded) {
    emscripten_sleep(100);
  }
#endif

  assert(userData.requestEnded);

  return userData.device;
}

WGPUAdapter requestAdapterSync(WGPUInstance instance,
                               WGPURequestAdapterOptions const *options) {
  struct UserData {
    WGPUAdapter adapter = nullptr;
    bool requestEnded = false;
  };

  UserData userData;

  auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status,
                                  WGPUAdapter adapter, char const *message,
                                  void *pUserData) {
    UserData &userData = *reinterpret_cast<UserData *>(pUserData);
    if (status == WGPURequestAdapterStatus_Success) {
      userData.adapter = adapter;
    } else {
      std::cout << "Could not get WebGPUdapter: " << message << std::endl;
    }
    userData.requestEnded = true;
  };

  wgpuInstanceRequestAdapter(instance, options, onAdapterRequestEnded,
                             (void *)&userData);

#ifdef __EMSCRIPTEN__
  while (!userData.requestEnded) {
    emscripten_sleep(100);
  }
#endif // __EMSCRIPTEN__
       //
  assert(userData.requestEnded);

  return userData.adapter;
}

void inspectDevice(WGPUDevice device) {
  std::vector<WGPUFeatureName> features;
  size_t featureCount = wgpuDeviceEnumerateFeatures(device, nullptr);
  features.resize(featureCount);
  wgpuDeviceEnumerateFeatures(device, features.data());

  std::cout << "Device features:" << std::endl;
  std::cout << std::hex;
  for (auto f : features) {
    std::cout << " - 0x" << f << std::endl;
  }
  std::cout << std::dec;

  WGPUSupportedLimits limits = {};
  limits.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_DAWN
  bool success = wgpuDeviceGetLimits(device, &limits) == WGPUStatus_Success;
#else
  bool success = wgpuDeviceGetLimits(device, &limits);
#endif

  if (success) {
    std::cout << "Device limits:" << std::endl;
    std::cout << " - maxTextureDimension1D: "
              << limits.limits.maxTextureDimension1D << std::endl;
    std::cout << " - maxTextureDimension2D: "
              << limits.limits.maxTextureDimension2D << std::endl;
    std::cout << " - maxTextureDimension3D: "
              << limits.limits.maxTextureDimension3D << std::endl;
    std::cout << " - maxTextureArrayLayers: "
              << limits.limits.maxTextureArrayLayers << std::endl;
    // [...] Extra device limits
  }
}

void inspectAdapter(WGPUAdapter adapter) {
#ifndef __EMSCRIPTEN__

  WGPUSupportedLimits supportedLimits = {};
  supportedLimits.nextInChain = nullptr;

#ifdef WEBGPU_BACKEN_DAWN
  bool success =
      wgpuAdapterGetLimits(adapter, &supportedLimits) == WGPUStatus_Success;
#else
  bool success = wgpuAdapterGetLimits(adapter, &supportedLimits);
#endif

  if (success) {
    std::cout << "Adapter limits:" << std::endl;
    std::cout << " - maxTextureDimension1D: "
              << supportedLimits.limits.maxTextureDimension1D << std::endl;
    std::cout << " - maxTextureDimension2D: "
              << supportedLimits.limits.maxTextureDimension2D << std::endl;
    std::cout << " - maxTextureDimension3D: "
              << supportedLimits.limits.maxTextureDimension3D << std::endl;
    std::cout << " - maxTextureArrayLayers: "
              << supportedLimits.limits.maxTextureArrayLayers << std::endl;
  }
#endif

  std::vector<WGPUFeatureName> features;

  size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);

  features.resize(featureCount);

  wgpuAdapterEnumerateFeatures(adapter, features.data());

  std::cout << "Adapter features:" << std::endl;
  std::cout << std::hex;
  for (auto f : features) {
    std::cout << " - 0x" << f << std::endl;
  }
  std::cout << std::dec;

  WGPUAdapterProperties properties = {};
  properties.nextInChain = nullptr;
  wgpuAdapterGetProperties(adapter, &properties);
  std::cout << "Adapter properties:" << std::endl;
  std::cout << " - vendorID: " << properties.vendorID << std::endl;
  if (properties.vendorName) {
    std::cout << " - vendorName: " << properties.vendorName << std::endl;
  }
  if (properties.architecture) {
    std::cout << " - architecture: " << properties.architecture << std::endl;
  }
  std::cout << " - deviceID: " << properties.deviceID << std::endl;
  if (properties.name) {
    std::cout << " - name: " << properties.name << std::endl;
  }
  if (properties.driverDescription) {
    std::cout << " - driverDescription: " << properties.driverDescription
              << std::endl;
  }
  std::cout << std::hex;
  std::cout << " - adapterType: 0x" << properties.adapterType << std::endl;
  std::cout << " - backendType: 0x" << properties.backendType << std::endl;
  std::cout << std::dec; // Restore decimal numbers
}

int main(int, char **) {
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

  WGPURequestAdapterOptions adapterOpts = {};
  adapterOpts.nextInChain = nullptr;
  WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);

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
  WGPUDevice device = requestDeviceSync(adapter, &deviceDesc);

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

  WGPUQueue queue = wgpuDeviceGetQueue(device);

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

  for (int i = 0; i < 5; ++i) {
#if defined(WEBGPU_BACKEND_DAWN)
    std::cout << "Waiting for tick..." << std::endl;
    wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
    std::cout << "Polling device..." << std::endl;
    wgpuDevicePoll(device, false, nullptr);
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
    std::cout << "Sleeping for 100ms..." << std::endl;
    emscripten_sleep(100);
#endif
  }

  // Clean up
  wgpuInstanceRelease(instance);
  wgpuDeviceRelease(device);
  wgpuQueueRelease(queue);

  return 0;
}
