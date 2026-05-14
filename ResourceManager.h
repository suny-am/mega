#pragma once
#include "Application.h"
#include <filesystem>
#include <vector>
#include <webgpu/webgpu.hpp>

class ResourceManager {
public:
  static bool LoadGeometry(const std::filesystem::path &path,
                           std::vector<float> &pointData,
                           std::vector<uint16_t> &indexData, int dimensions);

  static bool
  LoadGeometryFromObj(const std::filesystem::path &path,
                      std::vector<Application::VertexAttributes> &vertexData);

  static wgpu::ShaderModule LoadShaderModule(const std::filesystem::path &path,
                                             wgpu::Device device);
};
