#include "gpu-scene.h"
#include "webgpu-std-utils.hpp"
#include "webgpu-gltf-utils.h"

#include "resource-loaders/tiny_gltf.h"

#include <glm/glm/gtc/type_ptr.hpp>

#include <cassert>
#include <unordered_map>
#include <map>

using namespace wgpu;
using namespace tinygltf;
using namespace wgpu::gltf; // conversion utils

///////////////////////////////////////////////////////////////////////////////
// Public methods

void GpuScene::createFromModel(
	Device device,
	const tinygltf::Model& model,
	BindGroupLayout materialBindGroupLayout,
	BindGroupLayout nodeBindGroupLayout
) {
	destroy();

	initDevice(device);
	initBuffers(model);
	initTextures(model);
	initSamplers(model);
	initMaterials(model, materialBindGroupLayout);
	initNodes(model, nodeBindGroupLayout);
	initDrawCalls(model);
}

void GpuScene::draw(wgpu::RenderPassEncoder renderPass, uint32_t renderPipelineIndex) {
	for (const Node& node : m_nodes) {
		const Mesh& mesh = m_meshes[node.meshIndex];
		renderPass.setBindGroup(2, node.bindGroup, 0, nullptr);
		for (const MeshPrimitive& prim : mesh.primitives) {
			if (prim.renderPipelineIndex != renderPipelineIndex) continue;
			for (size_t layoutIdx = 0; layoutIdx < prim.attributeBufferViews.size(); ++layoutIdx) {
				const auto& view = prim.attributeBufferViews[layoutIdx];
				uint32_t slot = static_cast<uint32_t>(layoutIdx);
				if (view.bufferIndex != WGPU_LIMIT_U32_UNDEFINED) {
					renderPass.setVertexBuffer(slot, *m_buffers[view.bufferIndex], view.byteOffset, view.byteLength);
				}
				else {
					renderPass.setVertexBuffer(slot, *m_nullBuffer, 0, 4 * sizeof(float));
				}
			}
			renderPass.setBindGroup(1, m_materials[prim.materialIndex].bindGroup, 0, nullptr);
			assert(prim.indexBufferView.byteStride == 0 || prim.indexBufferView.byteStride == indexFormatByteSize(prim.indexFormat));
			renderPass.setIndexBuffer(
				*m_buffers[prim.indexBufferView.bufferIndex],
				prim.indexFormat,
				prim.indexBufferView.byteOffset + prim.indexBufferByteOffset,
				prim.indexBufferView.byteLength
			);
			renderPass.drawIndexed(prim.indexCount, 1, 0, 0, 0);
		}
	}
}

void GpuScene::destroy() {
	terminateDrawCalls();
	terminateNodes();
	terminateMaterials();
	terminateSamplers();
	terminateTextures();
	terminateBuffers();
}

///////////////////////////////////////////////////////////////////////////////
// Private methods

void GpuScene::initDevice(wgpu::Device device) {
	m_device = std::move(device);
	m_queue = m_device->getQueue();
}

void GpuScene::initBuffers(const tinygltf::Model& model) {
	for (const tinygltf::Buffer& buffer : model.buffers) {
		BufferDescriptor bufferDesc = Default;
		bufferDesc.label = buffer.name.c_str();
		bufferDesc.size = alignToNextMultipleOf(static_cast<uint32_t>(buffer.data.size()), 4);
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex | BufferUsage::Index;
		wgpu::Buffer gpuBuffer = m_device->createBuffer(bufferDesc);
		m_buffers.push_back(std::move(gpuBuffer));
		m_queue->writeBuffer(gpuBuffer, 0, buffer.data.data(), bufferDesc.size);
	}

	{
		BufferDescriptor bufferDesc = Default;
		bufferDesc.label = "Null Buffer";
		bufferDesc.size = static_cast<uint32_t>(4 * sizeof(float));
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex | BufferUsage::Index;
		m_nullBuffer = m_device->createBuffer(bufferDesc);
	}
}

void GpuScene::terminateBuffers() {
	for (wgpu::raii::Buffer b : m_buffers) {
		b->destroy();
	}
	m_buffers.clear();

	if (m_nullBuffer) {
		m_nullBuffer->destroy();
	}
}

void GpuScene::initTextures(const tinygltf::Model& model) {
	TextureDescriptor desc;
	for (const tinygltf::Image& image : model.images) {
		// Texture
		desc.label = image.name.c_str();
		desc.dimension = TextureDimension::_2D;
		desc.format = textureFormatToFloatFormat(textureFormatFromGltfImage(image));
		desc.sampleCount = 1;
		desc.size = { static_cast<uint32_t>(image.width) , static_cast<uint32_t>(image.height), 1 };
		desc.mipLevelCount = 1;// maxMipLevelCount2D(desc.size); // TODO -> upload mipmaps
		desc.usage = TextureUsage::CopyDst | TextureUsage::TextureBinding;
		desc.viewFormatCount = 0;
		desc.viewFormats = nullptr;
		wgpu::Texture gpuTexture = m_device->createTexture(desc);
		m_textures.push_back(gpuTexture);

		// View
		TextureViewDescriptor viewDesc;
		viewDesc.label = image.name.c_str();
		viewDesc.aspect = TextureAspect::All;
		viewDesc.baseMipLevel = 0;
		viewDesc.mipLevelCount = desc.mipLevelCount;
		viewDesc.baseArrayLayer = 0;
		viewDesc.arrayLayerCount = 1;
		viewDesc.dimension = TextureViewDimension::_2D;
		viewDesc.format = desc.format;
		wgpu::TextureView gpuTextureView = gpuTexture.createView(viewDesc);
		m_textureViews.push_back(gpuTextureView);

		// Upload
		ImageCopyTexture destination;
		destination.aspect = TextureAspect::All;
		destination.mipLevel = 0;
		destination.origin = { 0, 0, 0 };
		destination.texture = gpuTexture;
		TextureDataLayout sourceLayout;
		sourceLayout.offset = 0;
		uint32_t bitsPerPixel = textureFormatBitsPerTexel(desc.format);
		sourceLayout.bytesPerRow = bitsPerPixel * desc.size.width / 8;
		sourceLayout.rowsPerImage = desc.size.height;
		m_queue->writeTexture(destination, image.image.data(), image.image.size(), sourceLayout, desc.size);
	}

	// Default texture
	{
		m_defaultTextureIdx = static_cast<uint32_t>(m_textures.size());

		// Texture
		desc.label = "Default";
		desc.dimension = TextureDimension::_2D;
		desc.format = TextureFormat::RGBA8Unorm;
		desc.sampleCount = 1;
		desc.size = { 1, 1, 1 };
		desc.mipLevelCount = 1;// maxMipLevelCount2D(desc.size); // TODO -> upload mipmaps
		desc.usage = TextureUsage::CopyDst | TextureUsage::TextureBinding;
		desc.viewFormatCount = 0;
		desc.viewFormats = nullptr;
		wgpu::Texture gpuTexture = m_device->createTexture(desc);
		m_textures.push_back(gpuTexture);

		// View
		TextureViewDescriptor viewDesc;
		viewDesc.label = "Default";
		viewDesc.aspect = TextureAspect::All;
		viewDesc.baseMipLevel = 0;
		viewDesc.mipLevelCount = desc.mipLevelCount;
		viewDesc.baseArrayLayer = 0;
		viewDesc.arrayLayerCount = 1;
		viewDesc.dimension = TextureViewDimension::_2D;
		viewDesc.format = desc.format;
		wgpu::TextureView gpuTextureView = gpuTexture.createView(viewDesc);
		m_textureViews.push_back(gpuTextureView);

		// Upload
		ImageCopyTexture destination;
		destination.aspect = TextureAspect::All;
		destination.mipLevel = 0;
		destination.origin = { 0, 0, 0 };
		destination.texture = gpuTexture;
		TextureDataLayout sourceLayout;
		sourceLayout.offset = 0;
		uint32_t bitsPerPixel = textureFormatBitsPerTexel(desc.format);
		sourceLayout.bytesPerRow = bitsPerPixel * desc.size.width / 8;
		sourceLayout.rowsPerImage = desc.size.height;
		uint32_t data = 0;
		m_queue->writeTexture(destination, &data, 4, sourceLayout, desc.size);
	}

	for (const tinygltf::Texture& texture : model.textures) {
		m_sampledTextures.push_back(SampledTexture{
			static_cast<uint32_t>(texture.source),
			static_cast<uint32_t>(texture.sampler),
									});
	}
}

void GpuScene::terminateTextures() {
	m_sampledTextures.clear();
	for (wgpu::TextureView v : m_textureViews) {
		v.release();
	}
	m_textureViews.clear();
	for (wgpu::Texture t : m_textures) {
		t.destroy();
		t.release();
	}
	m_textures.clear();
}

void GpuScene::initSamplers(const tinygltf::Model& model) {
	SamplerDescriptor desc;
	for (const tinygltf::Sampler& sampler : model.samplers) {
		desc.label = sampler.name.c_str();
		desc.magFilter = filterModeFromGltf(sampler.magFilter);
		desc.minFilter = filterModeFromGltf(sampler.minFilter);
		desc.mipmapFilter = mipmapFilterModeFromGltf(sampler.minFilter);
		desc.addressModeU = addressModeFromGltf(sampler.wrapS);
		desc.addressModeV = addressModeFromGltf(sampler.wrapT);
		desc.addressModeW = AddressMode::Repeat;
		desc.lodMinClamp = 0.0;
		desc.lodMaxClamp = 1.0;
		desc.maxAnisotropy = 1.0;
		m_samplers.push_back(m_device->createSampler(desc));
	}

	m_defaultSamplerIdx = static_cast<uint32_t>(m_samplers.size());
	desc.label = "Default";
	m_samplers.push_back(m_device->createSampler(desc));
}

void GpuScene::terminateSamplers() {
	for (wgpu::Sampler s : m_samplers) {
		s.release();
	}
	m_samplers.clear();
}

void GpuScene::initMaterials(const tinygltf::Model& model, BindGroupLayout bindGroupLayout) {
	for (const tinygltf::Material& material : model.materials) {
		GpuScene::Material gpuMaterial;

		int baseColorSampledTextureIdx = material.pbrMetallicRoughness.baseColorTexture.index;
		int baseColorTextureIdx = -1;
		int baseColorSamplerIdx = -1;
		if (baseColorSampledTextureIdx >= 0) {
			baseColorTextureIdx = m_sampledTextures[baseColorSampledTextureIdx].textureIndex;
			baseColorSamplerIdx = m_sampledTextures[baseColorSampledTextureIdx].samplerIndex;
		}

		int metallicRoughnessSampledTextureIdx = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
		int metallicRoughnessTextureIdx = -1;
		int metallicRoughnessSamplerIdx = -1;
		if (metallicRoughnessSampledTextureIdx >= 0) {
			metallicRoughnessTextureIdx = m_sampledTextures[metallicRoughnessSampledTextureIdx].textureIndex;
			metallicRoughnessSamplerIdx = m_sampledTextures[metallicRoughnessSampledTextureIdx].samplerIndex;
		}

		int normalSampledTextureIdx = material.normalTexture.index;
		int normalTextureIdx = -1;
		int normalSamplerIdx = -1;
		if (normalSampledTextureIdx >= 0) {
			normalTextureIdx = m_sampledTextures[normalSampledTextureIdx].textureIndex;
			normalSamplerIdx = m_sampledTextures[normalSampledTextureIdx].samplerIndex;
		}

		// Uniforms
		BufferDescriptor bufferDesc;
		bufferDesc.label = material.name.c_str();
		bufferDesc.mappedAtCreation = false;
		bufferDesc.usage = BufferUsage::Uniform | BufferUsage::CopyDst;
		bufferDesc.size = sizeof(MaterialUniforms);
		gpuMaterial.uniformBuffer = m_device->createBuffer(bufferDesc);

		// Uniform Values
		gpuMaterial.uniforms.baseColorFactor = glm::make_vec4(material.pbrMetallicRoughness.baseColorFactor.data());
		gpuMaterial.uniforms.metallicFactor = material.pbrMetallicRoughness.metallicFactor;
		gpuMaterial.uniforms.roughnessFactor = material.pbrMetallicRoughness.roughnessFactor;
		gpuMaterial.uniforms.baseColorTexCoords =
			baseColorTextureIdx >= 0
			? static_cast<uint32_t>(material.pbrMetallicRoughness.baseColorTexture.texCoord)
			: WGPU_LIMIT_U32_UNDEFINED;
		gpuMaterial.uniforms.metallicRoughnessTexCoords =
			metallicRoughnessTextureIdx >= 0
			? static_cast<uint32_t>(material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord)
			: WGPU_LIMIT_U32_UNDEFINED;
		gpuMaterial.uniforms.normalTexCoords =
			normalTextureIdx >= 0
			? static_cast<uint32_t>(material.normalTexture.texCoord)
			: WGPU_LIMIT_U32_UNDEFINED;
		m_queue->writeBuffer(gpuMaterial.uniformBuffer, 0, &gpuMaterial.uniforms, sizeof(MaterialUniforms));

		// Bind Group
		std::vector<BindGroupEntry> bindGroupEntries(7, Default);
		bindGroupEntries[0].binding = 0;
		bindGroupEntries[0].buffer = gpuMaterial.uniformBuffer;
		bindGroupEntries[0].size = sizeof(MaterialUniforms);

		bindGroupEntries[1].binding = 1;
		int idx = baseColorTextureIdx >= 0 ? baseColorTextureIdx : m_defaultTextureIdx;
		bindGroupEntries[1].textureView = m_textureViews[idx];

		bindGroupEntries[2].binding = 2;
		idx = baseColorSamplerIdx >= 0 ? baseColorSamplerIdx : m_defaultSamplerIdx;
		bindGroupEntries[2].sampler = m_samplers[idx];

		bindGroupEntries[3].binding = 3;
		idx = metallicRoughnessTextureIdx >= 0 ? metallicRoughnessTextureIdx : m_defaultTextureIdx;
		bindGroupEntries[3].textureView = m_textureViews[idx];

		bindGroupEntries[4].binding = 4;
		idx = metallicRoughnessSamplerIdx >= 0 ? metallicRoughnessSamplerIdx : m_defaultSamplerIdx;
		bindGroupEntries[4].sampler = m_samplers[idx];

		bindGroupEntries[5].binding = 5;
		idx = normalTextureIdx >= 0 ? normalTextureIdx : m_defaultTextureIdx;
		bindGroupEntries[5].textureView = m_textureViews[idx];

		bindGroupEntries[6].binding = 6;
		idx = normalSamplerIdx >= 0 ? normalSamplerIdx : m_defaultSamplerIdx;
		bindGroupEntries[6].sampler = m_samplers[idx];

		BindGroupDescriptor bindGroupDesc;
		bindGroupDesc.label = material.name.c_str();
		bindGroupDesc.entryCount = static_cast<uint32_t>(bindGroupEntries.size());
		bindGroupDesc.entries = bindGroupEntries.data();
		bindGroupDesc.layout = bindGroupLayout;
		gpuMaterial.bindGroup = m_device->createBindGroup(bindGroupDesc);

		m_materials.push_back(gpuMaterial);
	}

	// Add default material
	{
		GpuScene::Material gpuMaterial;
		m_defaultMaterialIdx = static_cast<uint32_t>(m_materials.size());

		// Uniforms
		BufferDescriptor bufferDesc;
		bufferDesc.label = "Default Material";
		bufferDesc.mappedAtCreation = false;
		bufferDesc.usage = BufferUsage::Uniform | BufferUsage::CopyDst;
		bufferDesc.size = sizeof(MaterialUniforms);
		gpuMaterial.uniformBuffer = m_device->createBuffer(bufferDesc);

		// Uniform Values
		gpuMaterial.uniforms.baseColorFactor = { 1.0, 0.5, 0.5, 1.0 };
		gpuMaterial.uniforms.metallicFactor = 0.0;
		gpuMaterial.uniforms.roughnessFactor = 0.2;
		gpuMaterial.uniforms.baseColorTexCoords = WGPU_LIMIT_U32_UNDEFINED;
		m_queue->writeBuffer(gpuMaterial.uniformBuffer, 0, &gpuMaterial.uniforms, sizeof(MaterialUniforms));

		// Bind Group
		std::vector<BindGroupEntry> bindGroupEntries(7, Default);
		bindGroupEntries[0].binding = 0;
		bindGroupEntries[0].buffer = gpuMaterial.uniformBuffer;
		bindGroupEntries[0].size = sizeof(MaterialUniforms);

		bindGroupEntries[1].binding = 1;
		bindGroupEntries[1].textureView = m_textureViews[m_defaultTextureIdx];

		bindGroupEntries[2].binding = 2;
		bindGroupEntries[2].sampler = m_samplers[m_defaultSamplerIdx];

		bindGroupEntries[3].binding = 3;
		bindGroupEntries[3].textureView = m_textureViews[m_defaultTextureIdx];

		bindGroupEntries[4].binding = 4;
		bindGroupEntries[4].sampler = m_samplers[m_defaultSamplerIdx];

		bindGroupEntries[5].binding = 5;
		bindGroupEntries[5].textureView = m_textureViews[m_defaultTextureIdx];

		bindGroupEntries[6].binding = 6;
		bindGroupEntries[6].sampler = m_samplers[m_defaultSamplerIdx];

		BindGroupDescriptor bindGroupDesc;
		bindGroupDesc.label = "Default Material";
		bindGroupDesc.entryCount = static_cast<uint32_t>(bindGroupEntries.size());
		bindGroupDesc.entries = bindGroupEntries.data();
		bindGroupDesc.layout = bindGroupLayout;
		gpuMaterial.bindGroup = m_device->createBindGroup(bindGroupDesc);

		m_materials.push_back(gpuMaterial);
	}
}

void GpuScene::terminateMaterials() {
	for (Material& mat : m_materials) {
		mat.bindGroup.release();
		mat.uniformBuffer.destroy();
		mat.uniformBuffer.release();
	}
	m_materials.clear();
}

void GpuScene::initNodes(const tinygltf::Model& model, BindGroupLayout bindGroupLayout) {
	std::function<void(const std::vector<int>&, const glm::mat4&)> addNodes;
	addNodes = [&](const std::vector<int>& nodeIndices, const glm::mat4& parentGlobalTransform) {
		for (int idx : nodeIndices) {
			const tinygltf::Node& node = model.nodes[idx];
			std::cout << " - Adding node '" << node.name << "'" << std::endl;
			const glm::mat4 localTransform = nodeMatrix(node);
			const glm::mat4 globalTransform = parentGlobalTransform * localTransform;

			if (node.mesh > -1) {
				Node gpuNode;
				gpuNode.meshIndex = static_cast<uint32_t>(node.mesh);

				// Uniforms
				BufferDescriptor bufferDesc;
				bufferDesc.label = node.name.c_str();
				bufferDesc.mappedAtCreation = false;
				bufferDesc.usage = BufferUsage::Uniform | BufferUsage::CopyDst;
				bufferDesc.size = sizeof(NodeUniforms);
				gpuNode.uniformBuffer = m_device->createBuffer(bufferDesc);

				// Uniform Values
				gpuNode.uniforms.modelMatrix = globalTransform;
				m_queue->writeBuffer(gpuNode.uniformBuffer, 0, &gpuNode.uniforms, sizeof(NodeUniforms));

				// Bind Group
				std::vector<BindGroupEntry> bindGroupEntries(1, Default);
				bindGroupEntries[0].binding = 0;
				bindGroupEntries[0].buffer = gpuNode.uniformBuffer;
				bindGroupEntries[0].size = sizeof(NodeUniforms);

				BindGroupDescriptor bindGroupDesc;
				bindGroupDesc.label = node.name.c_str();
				bindGroupDesc.entryCount = static_cast<uint32_t>(bindGroupEntries.size());
				bindGroupDesc.entries = bindGroupEntries.data();
				bindGroupDesc.layout = bindGroupLayout;
				gpuNode.bindGroup = m_device->createBindGroup(bindGroupDesc);

				m_nodes.push_back(gpuNode);
			}

			// Recursive call
			addNodes(node.children, globalTransform);
		}
		};

	const tinygltf::Scene& scene = model.scenes[model.defaultScene];
	std::cout << "Loading scene '" << scene.name << "'..." << std::endl;
	glm::mat4 swapYandZ = { // because GLTF specifies Y as being up, and our viewer uses Z
		1.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, -1.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	};
	addNodes(scene.nodes, swapYandZ);
}

void GpuScene::terminateNodes() {
	for (Node& node : m_nodes) {
		node.bindGroup.release();
		node.uniformBuffer.destroy();
		node.uniformBuffer.release();
	}
	m_nodes.clear();
}

void GpuScene::initDrawCalls(const tinygltf::Model& model) {
	struct Comp {
		bool operator()(const GpuBufferView& a, const GpuBufferView& b) const {
			return std::tie(a.bufferIndex, a.byteOffset, a.byteLength, a.byteStride) < std::tie(b.bufferIndex, b.byteOffset, b.byteLength, b.byteStride);
		}
	};

	// We create our own set of buffer views, where attribute offsets cannot be
	// larger than the vertex buffer stride.
	std::vector<GpuBufferView> gpuBufferViews;
	std::map<GpuBufferView, uint32_t, Comp> gpuBufferViewLut;
	auto getOrCreateGpuBufferViewIndex = [&](const GpuBufferView& view) {
		auto it = gpuBufferViewLut.find(view);
		if (it != gpuBufferViewLut.end()) {
			return it->second;
		}
		else {
			uint32_t idx = static_cast<uint32_t>(gpuBufferViews.size());
			gpuBufferViews.push_back(view);
			gpuBufferViewLut[view] = idx;
			return idx;
		}
		};

	for (const tinygltf::Mesh& mesh : model.meshes) {
		Mesh gpuMesh;
		for (const tinygltf::Primitive& prim : mesh.primitives) {

			std::vector<std::tuple<std::string, uint32_t, VertexFormat>> semanticToLocation = {
				// GLTF Semantic, Shader input location, Default format
				{"POSITION", 0, VertexFormat::Float32x3},
				{"NORMAL", 1, VertexFormat::Float32x3},
				{"COLOR", 2, VertexFormat::Float32x3},
				{"TEXCOORD_0", 3, VertexFormat::Float32x2},
			};

			// We create one vertex buffer layout per bufferView
			std::vector<VertexBufferLayout> vertexBufferLayouts;
			// For each layout, there are multiple attributes
			std::vector<std::vector<VertexAttribute>> vertexBufferLayoutToAttributes;
			// For each layout, there is a single buffer view
			std::vector<GpuBufferView> vertexBufferLayoutToGpuBufferView;
			// And we keep the map from bufferView to layout index
			// NB: The index '-1' is used for attributes expected by the shader but not
			// provided by the GLTF file.
			std::unordered_map<int, size_t> gpuBufferViewToVertexBufferLayout;

			for (const auto& [attrSemantic, location, defaultFormat] : semanticToLocation) {
				int gpuBufferViewIdx = -1;
				VertexFormat format = defaultFormat;
				uint64_t attrByteOffset = 0;

				auto accessorIt = prim.attributes.find(attrSemantic);
				if (accessorIt != prim.attributes.end()) {
					Accessor accessor = model.accessors[accessorIt->second];
					const BufferView& bufferView = model.bufferViews[accessor.bufferView];
					format = vertexFormatFromAccessor(accessor);
					attrByteOffset = static_cast<uint64_t>(accessor.byteOffset);
					uint64_t byteStride =
						bufferView.byteStride != 0
						? bufferView.byteStride
						: vertexFormatByteSize(format);
					uint64_t bufferByteOffset = bufferView.byteOffset;

					// Prevent attribute offset from being larger than the stride
					uint64_t x = (attrByteOffset / byteStride) * byteStride;
					attrByteOffset -= x;
					bufferByteOffset += x;

					gpuBufferViewIdx = getOrCreateGpuBufferViewIndex(GpuBufferView{
						static_cast<uint32_t>(bufferView.buffer),
						bufferByteOffset,
						bufferView.byteLength,
						byteStride
																	 });
				}

				// Group attributes by bufferView
				size_t layoutIdx = 0;
				auto vertexBufferLayoutIt = gpuBufferViewToVertexBufferLayout.find(gpuBufferViewIdx);
				if (vertexBufferLayoutIt == gpuBufferViewToVertexBufferLayout.end()) {
					GpuBufferView view = gpuBufferViewIdx >= 0 ? gpuBufferViews[gpuBufferViewIdx] : GpuBufferView{};
					layoutIdx = vertexBufferLayouts.size();
					VertexBufferLayout layout;
					layout.arrayStride = view.byteStride;
					layout.stepMode = VertexStepMode::Vertex;
					vertexBufferLayouts.push_back(layout);
					vertexBufferLayoutToAttributes.push_back({});
					vertexBufferLayoutToGpuBufferView.push_back(view);
					gpuBufferViewToVertexBufferLayout[gpuBufferViewIdx] = layoutIdx;
				}
				else {
					layoutIdx = vertexBufferLayoutIt->second;
				}

				VertexAttribute attrib;
				attrib.shaderLocation = location;
				attrib.format = format;
				attrib.offset = attrByteOffset;
				vertexBufferLayoutToAttributes[layoutIdx].push_back(attrib);
			}

			// Index data
			const Accessor& indexAccessor = model.accessors[prim.indices];
			const BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
			IndexFormat indexFormat = indexFormatFromAccessor(indexAccessor);
			assert(indexFormat != IndexFormat::Undefined);
			assert(indexAccessor.type == TINYGLTF_TYPE_SCALAR);

			RenderPipelineSettings renderPipelineSettings = {
				vertexBufferLayoutToAttributes,
				vertexBufferLayouts,
				primitiveTopologyFromGltf(prim)
			};

			gpuMesh.primitives.push_back(MeshPrimitive{
				vertexBufferLayoutToGpuBufferView,
				GpuBufferView{
					static_cast<uint32_t>(indexBufferView.buffer),
					indexBufferView.byteOffset,
					indexBufferView.byteLength,
					indexBufferView.byteStride
				},
				static_cast<uint32_t>(indexAccessor.byteOffset),
				indexFormat,
				static_cast<uint32_t>(indexAccessor.count),
				prim.material >= 0 ? static_cast<uint32_t>(prim.material) : m_defaultMaterialIdx,
				getOrCreateRenderPipelineIndex(renderPipelineSettings)
										 });
		}
		m_meshes.push_back(std::move(gpuMesh));
	}
}

bool GpuScene::isCompatible(const RenderPipelineSettings& a, const RenderPipelineSettings& b) const {
	if (a.vertexBufferLayouts.size() != b.vertexBufferLayouts.size()) return false;
	assert(a.vertexAttributes.size() == a.vertexBufferLayouts.size());
	assert(b.vertexAttributes.size() == b.vertexBufferLayouts.size());

	if (a.primitiveTopology != b.primitiveTopology) return false;

	for (int bufferIdx = 0; bufferIdx < a.vertexBufferLayouts.size(); ++bufferIdx) {
		if (a.vertexAttributes[bufferIdx].size() != b.vertexAttributes[bufferIdx].size()) return false;
		const auto& bufferLayoutA = a.vertexBufferLayouts[bufferIdx];
		const auto& bufferLayoutB = b.vertexBufferLayouts[bufferIdx];
		if (bufferLayoutA.arrayStride != bufferLayoutB.arrayStride) return false;
		if (bufferLayoutA.stepMode != bufferLayoutB.stepMode) return false;

		std::unordered_map<uint32_t, uint32_t> locationToFormatA;
		std::unordered_map<uint32_t, uint64_t> locationToOffsetA;
		std::unordered_map<uint32_t, uint32_t> locationToFormatB;
		std::unordered_map<uint32_t, uint64_t> locationToOffsetB;
		for (int attrIdx = 0; attrIdx < a.vertexAttributes[bufferIdx].size(); ++attrIdx) {
			const auto& attrA = a.vertexAttributes[bufferIdx][attrIdx];
			const auto& attrB = b.vertexAttributes[bufferIdx][attrIdx];
			locationToFormatA[attrA.shaderLocation] = (uint32_t)attrA.format;
			locationToOffsetA[attrA.shaderLocation] = attrA.offset;
			locationToFormatB[attrB.shaderLocation] = (uint32_t)attrB.format;
			locationToOffsetB[attrB.shaderLocation] = attrB.offset;
		}
		if (locationToFormatA != locationToFormatB) return false;
		if (locationToOffsetA != locationToOffsetB) return false;
	}

	return true;
}

uint32_t GpuScene::getOrCreateRenderPipelineIndex(const RenderPipelineSettings& newSettings) {
	for (uint32_t idx = 0; idx < m_renderPipelines.size(); ++idx) {
		const RenderPipelineSettings& settings = m_renderPipelines[idx];
		if (isCompatible(settings, newSettings)) {
			return idx;
		}
	}

	// No appropriate render pipeline was found, register a new one
	m_renderPipelines.push_back(newSettings);
	return static_cast<uint32_t>(m_renderPipelines.size() - 1);
}

void GpuScene::terminateDrawCalls() {
	m_meshes.clear();
	m_renderPipelines.clear();
}

uint32_t GpuScene::renderPipelineCount() const {
	return static_cast<uint32_t>(m_renderPipelines.size());
}

std::vector<wgpu::VertexBufferLayout> GpuScene::vertexBufferLayouts(uint32_t renderPipelineIndex) const {
	const auto& rp = m_renderPipelines[renderPipelineIndex];
	std::vector<wgpu::VertexBufferLayout> vertexBufferLayouts = rp.vertexBufferLayouts;

	// Assign pointer addresses (should be done upon foreign access)
	for (size_t layoutIdx = 0; layoutIdx < rp.vertexBufferLayouts.size(); ++layoutIdx) {
		VertexBufferLayout& layout = vertexBufferLayouts[layoutIdx];
		const auto& attributes = rp.vertexAttributes[layoutIdx];
		layout.attributeCount = static_cast<uint32_t>(attributes.size());
		layout.attributes = attributes.data();
	}

	return vertexBufferLayouts;
}

PrimitiveTopology GpuScene::primitiveTopology(uint32_t renderPipelineIndex) const {
	return m_renderPipelines[renderPipelineIndex].primitiveTopology;
}