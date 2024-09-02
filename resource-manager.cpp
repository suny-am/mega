#include "application.h"
#include "resource-manager.h"
#include <webgpu/webgpu.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "resource-loaders/tiny_obj_loader.h"
#include "resource-loaders/stb_image.h"
#include <filesystem>
#include <fstream>
#include <nfd.h>

using std::cout;
using std::cerr;
using std::endl;
using std::vector;
using std::string;

using namespace wgpu;

static void writeMipMaps(
    Device device,
    Texture texture,
    Extent3D textureSize,
    [[maybe_unused]] uint32_t mipLevelcount,
    const unsigned char* pixelData
);

uint32_t bit_width(uint32_t m);

ShaderModule ResourceManager::loadShaderModule(const path& path, Device device) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return nullptr;
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    string shaderSource(size, ' ');
    file.seekg(0);
    file.read(shaderSource.data(), size);

    ShaderModuleWGSLDescriptor shaderCodeDesc{};
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = SType::ShaderModuleWGSLDescriptor;
    shaderCodeDesc.code = shaderSource.c_str();
    ShaderModuleDescriptor shaderDesc{};
    shaderDesc.hintCount = 0;
    shaderDesc.hints = nullptr;
    shaderDesc.nextInChain = &shaderCodeDesc.chain;
    return device.createShaderModule(shaderDesc);
}

bool ResourceManager::loadGeometryFromObj(const path& path, std::vector<VertexAttributes>& vertexData) {
    tinyobj::attrib_t attrib;
    vector<tinyobj::shape_t> shapes;
    vector<tinyobj::material_t> materials;

    string warn;
    string err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str());

    if (!warn.empty()) {
        cout << warn << endl;
    }

    if (!err.empty()) {
        cout << err << endl;
    }

    if (!ret) {
        return false;
    }

    vertexData.clear();
    for (const auto& shape : shapes) {
        size_t offset = vertexData.size();
        vertexData.resize(offset + shape.mesh.indices.size());
        for (size_t i = 0; i < shape.mesh.indices.size(); ++i) {

            const tinyobj::index_t& idx = shape.mesh.indices[i];

            vertexData[offset + i].position = {
                attrib.vertices[3 * idx.vertex_index + 0],
                -attrib.vertices[3 * idx.vertex_index + 2],
                attrib.vertices[3 * idx.vertex_index + 1],
            };

            vertexData[offset + i].normal = {
                attrib.normals[3 * idx.normal_index + 0],
                -attrib.normals[3 * idx.normal_index + 2],
                attrib.normals[3 * idx.normal_index + 1],
            };

            vertexData[offset + i].objectColor = {
                attrib.colors[3 * idx.vertex_index + 0],
                attrib.colors[3 * idx.vertex_index + 1],
                attrib.colors[3 * idx.vertex_index + 2]
            };

            vertexData[offset + i].uv = {
                attrib.texcoords[2 * idx.texcoord_index + 0],
                1 - attrib.texcoords[2 * idx.texcoord_index + 1],
            };
        }
    }

    populateTextureFrameAttributes(vertexData);
    return true;
}

Texture ResourceManager::loadTexture(const path& path, Device device, TextureView* pTextureView) {
    int width, height, channels;
    unsigned char* pixelData = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (nullptr == pixelData) return nullptr;

    TextureDescriptor textureDesc;
    textureDesc.dimension = TextureDimension::_2D;
    textureDesc.size = { (unsigned int)width, (unsigned int)height, 1 };
    textureDesc.format = TextureFormat::RGBA8Unorm;
    textureDesc.mipLevelCount = bit_width(std::max(textureDesc.size.width, textureDesc.size.height));
    textureDesc.sampleCount = 1;
    textureDesc.usage = TextureUsage::TextureBinding | TextureUsage::CopyDst;
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;
    Texture texture = device.createTexture(textureDesc);

    writeMipMaps(device, texture, textureDesc.size, textureDesc.mipLevelCount, pixelData);

    stbi_image_free(pixelData);

    if (pTextureView) {
        TextureViewDescriptor textureViewDesc;
        textureViewDesc.aspect = TextureAspect::All;
        textureViewDesc.baseArrayLayer = 0;
        textureViewDesc.arrayLayerCount = 1;
        textureViewDesc.baseMipLevel = 0;
        textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
        textureViewDesc.dimension = TextureViewDimension::_2D;
        textureViewDesc.format = textureDesc.format;
        *pTextureView = texture.createView(textureViewDesc);
    }

    return texture;
}

ResourceManager::path ResourceManager::loadGeometryFromFile() {
    NFD_Init();

    nfdu8char_t* outPath;
    nfdu8filteritem_t filters[1] = { { "Model file", "obj" } };
    nfdopendialogu8args_t args = { };
    args.filterList = filters;
    args.filterCount = 1;
    nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);
    path modelPath;
    if (result == NFD_OKAY)
    {
        modelPath = outPath;
        NFD_FreePathU8(outPath);
    }
    else if (result == NFD_CANCEL)
    {
        puts("File load operation cancelled.");
    }
    else
    {
        printf("File Error: %s\n", NFD_GetError());
    }

    NFD_Quit();
    return modelPath;
}

void ResourceManager::populateTextureFrameAttributes(std::vector<VertexAttributes>& vertexData) {
    size_t triangleCount = vertexData.size() / 3;
    for (size_t t = 0; t < triangleCount; ++t) {
        VertexAttributes* v = &vertexData[3 * t];

        for (int k = 0; k < 3; ++k) {
            mat3x3 TBN = computeTBN(v, v[k].normal);
            v[k].tangent = TBN[0];
            v[k].bitangent = TBN[1];
            v[k].normal = TBN[2];
        }
    }
}

glm::mat3x3 ResourceManager::computeTBN(const VertexAttributes corners[3], const vec3& expectedN) {
    vec3 ePos1 = corners[1].position - corners[0].position;
    vec3 ePos2 = corners[2].position - corners[0].position;

    vec2 eUV1 = corners[1].uv - corners[0].uv;
    vec2 eUV2 = corners[2].uv - corners[0].uv;

    vec3 T = normalize(ePos1 * eUV2.y - ePos2 * eUV1.y);
    vec3 B = normalize(ePos2 * eUV1.x - ePos1 * eUV2.x);
    vec3 N = cross(T, B);

    if (dot(N, expectedN) < 0.0) {
        T = -T;
        B = -B;
        N = -N;
    }

    N = expectedN;
    T = normalize(T - dot(T, N) * N);
    B = cross(N, T);

    return mat3x3(T, B, N);
}

uint32_t bit_width(uint32_t m) {
    if (m == 0) return 0;
    else { uint32_t w = 0; while (m >>= 1) ++w; return w; }
}

static void writeMipMaps(
    Device device,
    Texture texture,
    Extent3D textureSize,
    uint32_t mipLevelCount,
    const unsigned char* pixelData)
{
    Queue queue = device.getQueue();

    // Arguments telling which part of the texture we upload to
    ImageCopyTexture destination;
    destination.texture = texture;
    destination.origin = { 0, 0, 0 };
    destination.aspect = TextureAspect::All;

    // Arguments telling how the C++ side pixel memory is laid out
    TextureDataLayout source;
    source.offset = 0;

    // Create image data
    Extent3D mipLevelSize = textureSize;
    std::vector<unsigned char> previousLevelPixels;
    Extent3D previousMipLevelSize;
    for (uint32_t level = 0; level < mipLevelCount; ++level) {
        // Pixel data for the current level
        std::vector<unsigned char> pixels(4 * mipLevelSize.width * mipLevelSize.height);
        if (level == 0) {
            // We cannot really avoid this copy since we need this
            // in previousLevelPixels at the next iteration
            memcpy(pixels.data(), pixelData, pixels.size());
        }
        else {
            // Create mip level data
            for (uint32_t i = 0; i < mipLevelSize.width; ++i) {
                for (uint32_t j = 0; j < mipLevelSize.height; ++j) {
                    unsigned char* p = &pixels[4 * (j * mipLevelSize.width + i)];
                    // Get the corresponding 4 pixels from the previous level
                    unsigned char* p00 = &previousLevelPixels[4 * ((2 * j + 0) * previousMipLevelSize.width + (2 * i + 0))];
                    unsigned char* p01 = &previousLevelPixels[4 * ((2 * j + 0) * previousMipLevelSize.width + (2 * i + 1))];
                    unsigned char* p10 = &previousLevelPixels[4 * ((2 * j + 1) * previousMipLevelSize.width + (2 * i + 0))];
                    unsigned char* p11 = &previousLevelPixels[4 * ((2 * j + 1) * previousMipLevelSize.width + (2 * i + 1))];
                    // Average
                    p[0] = (p00[0] + p01[0] + p10[0] + p11[0]) / 4;
                    p[1] = (p00[1] + p01[1] + p10[1] + p11[1]) / 4;
                    p[2] = (p00[2] + p01[2] + p10[2] + p11[2]) / 4;
                    p[3] = (p00[3] + p01[3] + p10[3] + p11[3]) / 4;
                }
            }
        }

        // Upload data to the GPU texture
        destination.mipLevel = level;
        source.bytesPerRow = 4 * mipLevelSize.width;
        source.rowsPerImage = mipLevelSize.height;
        queue.writeTexture(destination, pixels.data(), pixels.size(), source, mipLevelSize);

        previousLevelPixels = std::move(pixels);
        previousMipLevelSize = mipLevelSize;
        mipLevelSize.width /= 2;
        mipLevelSize.height /= 2;
    }

    queue.release();
}