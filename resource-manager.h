#pragma once
#include <webgpu/webgpu.hpp>
#include <glm/glm/glm.hpp>
#include <glm/glm/ext.hpp>
#include <filesystem>

using namespace wgpu;

class ResourceManager {
public:
    using path = std::filesystem::path;
    using mat3x3 = glm::mat3x3;
    using vec3 = glm::vec3;
    using vec2 = glm::vec2;

    struct VertexAttributes {
        vec3 position;
        vec3 tangent;
        vec3 bitangent;
        vec3 normal;
        vec3 worldColor;
        vec3 objectColor;
        vec2 uv;
    };

    static ShaderModule loadShaderModule(const path& path, Device device);

    static bool loadGeometryFromObj(const path& path, std::vector<VertexAttributes>& vertexData);

    static bool loadGeometryFromGltf(const path& path, tinygltf::Model& model);

    static Texture loadTexture(const path& path, Device device, TextureView* pTextureView = nullptr);

    static path openFileDialog();

private:
    static mat3x3 computeTBN(const VertexAttributes corners[3], const vec3& expectedN);
    static void populateTextureFrameAttributes(std::vector<VertexAttributes>& vertexData);
};