#include "ResourceManager.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <fstream>
#include <sstream>
#include <string>

using namespace wgpu;

bool ResourceManager::LoadGeometry(const std::filesystem::path &path,
                                   std::vector<float> &pointData,
                                   std::vector<uint16_t> &indexData,
                                   int dimensions) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return false;
  }

  pointData.clear();
  indexData.clear();

  enum class Section {
    None,
    Points,
    Indices,
  };
  Section currentSection = Section::None;

  float value;
  uint16_t index;
  std::string line;
  while (!file.eof()) {
    getline(file, line);

    // overcome the `CRLF` problem
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    if (line == "[points]") {
      currentSection = Section::Points;
    } else if (line == "[indices]") {
      currentSection = Section::Indices;
    } else if (line[0] == '#' || line.empty()) {
      // Do nothing, this is a comment
    } else if (currentSection == Section::Points) {
      std::istringstream iss(line);
      // Get x, y, r, g, b
      for (int i = 0; i < dimensions + 3; ++i) {
        iss >> value;
        pointData.push_back(value);
      }
    } else if (currentSection == Section::Indices) {
      std::istringstream iss(line);
      // Get corners #0 #1 and #2
      for (int i = 0; i < 3; ++i) {
        iss >> index;
        indexData.push_back(index);
      }
    }
  }
  return true;
}

bool ResourceManager::LoadGeometryFromObj(
    const std::filesystem::path &path,
    std::vector<Application::VertexAttributes> &vertexData) {

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string warn;
  std::string err;

  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                              path.string().c_str());

  if (!warn.empty()) {
    std::cerr << "WARN: " << warn << std::endl;
  }

  if (!err.empty()) {
    std::cerr << "ERR: " << err << std::endl;
  }

  if (!ret) {
    return false;
  }

  vertexData.clear();

  for (const auto &shape : shapes) {
    size_t offset = vertexData.size();
    vertexData.resize(shape.mesh.indices.size());

    for (size_t i = 0; i < shape.mesh.indices.size(); ++i) {
      const tinyobj::index_t &idx = shape.mesh.indices[i];

      vertexData[offset + i].position = {
          attrib.vertices[3 * idx.vertex_index + 0],
          -attrib.vertices[3 * idx.vertex_index + 2],
          attrib.vertices[3 * idx.vertex_index + 1]};

      vertexData[offset + i].normal = {
          attrib.normals[3 * idx.normal_index + 0],
          -attrib.normals[3 * idx.normal_index + 2],
          attrib.normals[3 * idx.normal_index + 1]};

      vertexData[offset + i].color = {attrib.colors[3 * idx.vertex_index + 0],
                                      attrib.colors[3 * idx.vertex_index + 1],
                                      attrib.colors[3 * idx.vertex_index + 2]};
    }
  }

  return true;
}

ShaderModule
ResourceManager::LoadShaderModule(const std::filesystem::path &path,
                                  Device device) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return nullptr;
  }
  file.seekg(0, std::ios::end);
  size_t size = file.tellg();
  std::string shaderSource(size, ' ');
  file.seekg(0);
  file.read(shaderSource.data(), size);

  ShaderModuleWGSLDescriptor shaderCodeDesc{};
  shaderCodeDesc.chain.next = nullptr;
  shaderCodeDesc.chain.sType = SType::ShaderModuleWGSLDescriptor;
  shaderCodeDesc.code = shaderSource.c_str();

  ShaderModuleDescriptor shaderDesc{};
#ifdef WEBGPU_BACKEND_WGPU
  shaderDesc.hintCount = 0;
  shaderDesc.hints = nullptr;
#endif
  shaderDesc.nextInChain = &shaderCodeDesc.chain;
  return device.createShaderModule(shaderDesc);
}
