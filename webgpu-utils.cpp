#include "webgpu-utils.h"
#include <cassert>
#include <cstddef>
#include <iostream>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif // __EMSCRIPTEN__

void inspectAdapter(Adapter adapter) {
#ifndef __EMSCRIPTEN__

  SupportedLimits supportedLimits = {};

#ifdef WEBGPU_BACKEND_DAWN
  bool success = adapter.getLimits(&supportedLimits) = Status::Success;
#else
  bool success = adapter.getLimits(&supportedLimits);
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

  size_t featureCount = adapter.enumerateFeatures(nullptr);
  std::vector<FeatureName> features(featureCount, FeatureName::Undefined);
  adapter.enumerateFeatures(features.data());

  std::cout << "Adapter features:" << std::endl;
  std::cout << std::hex;
  for (auto f : features) {
    std::cout << " - 0x" << f << std::endl;
  }
  std::cout << std::dec;

  AdapterProperties properties = {};
  adapter.getProperties(&properties);
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

void inspectDevice(Device device) {
  size_t featureCount = device.enumerateFeatures(nullptr);
  std::vector<FeatureName> features(featureCount, FeatureName::Undefined);
  device.enumerateFeatures(features.data());

  std::cout << "Device features:" << std::endl;
  std::cout << std::hex;
  for (auto f : features) {
    std::cout << " - 0x" << f << std::endl;
  }
  std::cout << std::dec;

  SupportedLimits limits = {};
  limits.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_DAWN
  bool success = device.getLimits(&limits) = Status::Success;
#else
  bool success = device.getLimits(&limits);
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
    // TODO: Extra device limits go here
  }
}
