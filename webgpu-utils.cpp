#include "webgpu-utils.h"
#include <cassert>
#include <iostream>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif // __EMSCRIPTEN__

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
