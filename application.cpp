
#include "application.h"
#include "controls.h"
#include "resource-manager.h"
#include "ui-manager.h"

#include "webgpu-utils/webgpu-std-utils.hpp"

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#include <glm/glm/glm.hpp>
#include <glm/glm/ext.hpp>

#include <imgui/imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#include <iostream>
#include <cassert>
#include <filesystem>
#include <sstream>
#include <string>
#include <array>

using namespace wgpu;
using VertexAttributes = ResourceManager::VertexAttributes;

constexpr float PI = 3.14159265358979323846f;

/**
 * Public methods
 */

bool Application::onInit() {
	if (!initWindowAndDevice()) return false;
	if (!initSurfaceConfiguration()) return false;
	if (!initDepthBuffer()) return false;
	if (!initBindGroupLayouts()) return false;
	// m_filePath = (ResourceManager::path)RESOURCE_DIR "/scenes/triangle.gltf";
	m_filePath = (ResourceManager::path)RESOURCE_DIR "/scenes/box.gltf";
	// m_filePath = (ResourceManager::path)RESOURCE_DIR "/scenes/BusterDrone.gltf";
	// m_filePath = (ResourceManager::path)RESOURCE_DIR "/scenes/DamagedHelmet.glb";
	if (!initGeometry(m_filePath)) return false;
	if (!initRenderPipelines()) return false;
	if (!initUniforms()) return false;
	if (!initLightingUniforms()) return false;
	if (!initBindGroup()) return false;
	if (!UiManager::init(m_window, *m_device, m_surfaceFormat, m_depthTextureFormat)) return false;
	return true;
}

void Application::onFinish() {
	UiManager::shutdown();
	terminateLightingUniforms();
	terminateUniforms();
	terminateRenderPipelines();
	terminateGeometry();
	terminateDepthBuffer();
	terminateWindowAndDevice();
}

void Application::onFrame() {

	if (m_filePathHasChanged) {
		if (updateGeometry()) {
			m_filePathHasChanged = false;
		}
	}

	glfwPollEvents();
	// Controls::updateDragInertia(*&m_drag, *&m_cameraState);
	updateLightingUniforms();

	// Update uniform buffer
	m_uniforms.time = static_cast<float>(glfwGetTime());
	m_queue->writeBuffer(*m_uniformBuffer, offsetof(GlobalUniforms, time), &m_uniforms.time, sizeof(GlobalUniforms::time));

	TextureView nextTexture = getNextSurfaceTextureView();
	if (!nextTexture) {
		std::cerr << "Could not acquire next texture from surface configuration" << std::endl;
		return;
	}

	CommandEncoderDescriptor commandEncoderDesc;
	commandEncoderDesc.label = "Command Encoder";
	CommandEncoder encoder = m_device->createCommandEncoder(commandEncoderDesc);

	RenderPassDescriptor renderPassDesc{};

	RenderPassColorAttachment renderPassColorAttachment{};
	renderPassColorAttachment.view = nextTexture;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = LoadOp::Clear;
	renderPassColorAttachment.storeOp = StoreOp::Store;
	renderPassColorAttachment.clearValue = Color{ m_uniforms.worldColor.r,m_uniforms.worldColor.g, m_uniforms.worldColor.b,m_uniforms.worldColor.w };
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;

	RenderPassDepthStencilAttachment depthStencilAttachment;
	depthStencilAttachment.view = *m_depthTextureView;
	depthStencilAttachment.depthClearValue = 1.0f;
	depthStencilAttachment.depthLoadOp = LoadOp::Clear;
	depthStencilAttachment.depthStoreOp = StoreOp::Store;
	depthStencilAttachment.depthReadOnly = false;
	depthStencilAttachment.stencilClearValue = 0;
#ifdef WEBGPU_BACKEND_WGPU
	depthStencilAttachment.stencilLoadOp = LoadOp::Clear;
	depthStencilAttachment.stencilStoreOp = StoreOp::Store;
#else
	depthStencilAttachment.stencilLoadOp = LoadOp::Undefined;
	depthStencilAttachment.stencilStoreOp = StoreOp::Undefined;
#endif
	depthStencilAttachment.stencilReadOnly = true;

	renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

	renderPassDesc.timestampWrites = nullptr;
	RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);

	for (uint32_t pipelineIdx = 0; pipelineIdx < m_pipelines.size(); ++pipelineIdx) {
		renderPass.setPipeline(m_pipelines[pipelineIdx]);
		renderPass.setBindGroup(0, *m_bindGroup, 0, nullptr);

		m_gpuScene.draw(renderPass, pipelineIdx);
	}

	// We add the GUI drawing commands to the render pass
	UiManager::update(renderPass, m_uniforms, m_lightingUniforms, m_lightingUniformsChanged, m_filePath, m_filePathHasChanged);

	renderPass.end();
	renderPass.release();

	nextTexture.release();

	CommandBufferDescriptor cmdBufferDescriptor{};
	cmdBufferDescriptor.label = "Command buffer";
	CommandBuffer command = encoder.finish(cmdBufferDescriptor);
	encoder.release();

	m_queue->submit(command);
	command.release();

	m_surface->present();

#ifdef WEBGPU_BACKEND_DAWN
	// Check for pending error callbacks
	m_device->tick();
#endif
}

bool Application::isRunning() {
	return !glfwWindowShouldClose(m_window);
}

void Application::onResize() {
	// Terminate in reverse order
	terminateDepthBuffer();

	// Re-init
	initSurfaceConfiguration();
	initDepthBuffer();

	updateProjectionMatrix();
}

void Application::onMouseMove(double xPos, double yPos) {
	if (m_drag.active) {
		Controls::updateMouseMove(xPos, yPos, *&m_drag, *&m_cameraState);
		updateViewMatrix();
		Controls::smoothOut(xPos, yPos, *&m_drag);
	}
}

void Application::onMouseButton(int button, int action, int mods) {
	Controls::updateMouseButton(button, action, mods, *&m_drag, *&m_cameraState, m_window);
}

void Application::onScroll(double xOffset, double yOffset) {
	Controls::updateScroll(xOffset, yOffset, *&m_drag, *&m_cameraState);
	updateViewMatrix();
}

/**
 * Private methods
 */

bool Application::initWindowAndDevice() {
	m_instance = createInstance(InstanceDescriptor{});
	if (!m_instance) {
		std::cerr << "Could not initialize WebGPU!" << std::endl;
		return false;
	}

	if (!glfwInit()) {
		std::cerr << "Could not initialize GLFW!" << std::endl;
		return false;
	}

	auto monitor = glfwGetPrimaryMonitor();
	int monWidth, monHeight;
	glfwGetMonitorWorkarea(monitor, nullptr, nullptr, &monWidth, &monHeight);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_window = glfwCreateWindow(monWidth, monHeight, "Mega Render Engine", NULL, NULL);
	if (!m_window) {
		std::cerr << "Could not open window!" << std::endl;
		return false;
	}

	std::cout << "Requesting adapter..." << std::endl;
	*m_surface = glfwGetWGPUSurface(*m_instance, m_window);
	RequestAdapterOptions adapterOpts{};
	adapterOpts.compatibleSurface = *m_surface;
	Adapter adapter = m_instance->requestAdapter(adapterOpts);
	std::cout << "Got adapter: " << adapter << std::endl;

	SupportedLimits supportedLimits;
	adapter.getLimits(&supportedLimits);

	std::cout << "Requesting device..." << std::endl;
	RequiredLimits requiredLimits = Default;
	requiredLimits.limits.maxVertexAttributes = 4;
	requiredLimits.limits.maxVertexBuffers = 4;
	requiredLimits.limits.maxBufferSize = 1500000 * sizeof(VertexAttributes);
	requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.maxInterStageShaderComponents = 11;
	requiredLimits.limits.maxBindGroups = 3;
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 2;
	requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);

	int maxTextureDimensions = 2048;
	int monCount;
	auto monitors = glfwGetMonitors(&monCount);
	for (int monIdx = 0; monIdx < monCount; ++monIdx) {
		int monWidth;
		glfwGetMonitorWorkarea(monitors[monIdx], nullptr, nullptr, &monWidth, nullptr);
		if (maxTextureDimensions < monWidth) {
			maxTextureDimensions = monWidth;
		}
	}

	requiredLimits.limits.maxTextureDimension1D = maxTextureDimensions;
	requiredLimits.limits.maxTextureDimension2D = maxTextureDimensions;
	requiredLimits.limits.maxTextureArrayLayers = 1;
	requiredLimits.limits.maxSampledTexturesPerShaderStage = 3;
	requiredLimits.limits.maxSamplersPerShaderStage = 3;

	DeviceDescriptor deviceDesc;
	deviceDesc.label = "My Device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = &requiredLimits;
	deviceDesc.defaultQueue.label = "Default Device";
	m_device = adapter.requestDevice(deviceDesc);
	std::cout << "Got device: " << m_device << std::endl;

	// Add an error callback for more debug info
	m_errorCallbackHandle = m_device->setUncapturedErrorCallback([](ErrorType type, char const* message) {
		std::cout << "Device error: type " << type;
		if (message)  std::cout << " (message: " << message << ")";
		std::cout << std::endl;
		exit(1);
																 });

	m_queue = m_device->getQueue();

#ifdef WEBGPU_BACKEND_WGPU
	m_surfaceFormat = m_surface->getPreferredFormat(adapter);
#else
	m_surfaceFormat = TextureFormat::BGRA8Unorm;
#endif

	// Add window callbacks
	// Set the user pointer to be "this"
	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int, int) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onResize();
								   });
	glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xPos, double yPos) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onMouseMove(xPos, yPos);
							 });
	glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onMouseButton(button, action, mods);
							   });
	glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, double yoffset) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onScroll(xoffset, yoffset);
						  });

	adapter.release();
	return m_device;
}

void Application::terminateWindowAndDevice() {
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

bool Application::initSurfaceConfiguration()
{
	// Get the current size of the window's framebuffer
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	SurfaceConfiguration surfaceConfig = {};

	surfaceConfig.width = static_cast<uint32_t>(width);
	surfaceConfig.height = static_cast<uint32_t>(height);
	surfaceConfig.usage = TextureUsage::RenderAttachment;
	surfaceConfig.format = m_surfaceFormat;
	surfaceConfig.viewFormatCount = 0;
	surfaceConfig.viewFormats = nullptr;
	surfaceConfig.device = *m_device;
	surfaceConfig.presentMode = PresentMode::Fifo;
	surfaceConfig.alphaMode = CompositeAlphaMode::Auto;

	m_surface->configure(surfaceConfig);

	return m_surface;
}

bool Application::initDepthBuffer() {
	// Get the current size of the window's framebuffer:
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	// Create the depth texture
	TextureDescriptor depthTextureDesc;
	depthTextureDesc.dimension = TextureDimension::_2D;
	depthTextureDesc.format = m_depthTextureFormat;
	depthTextureDesc.mipLevelCount = 1;
	depthTextureDesc.sampleCount = 1;
	depthTextureDesc.size = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
	depthTextureDesc.usage = TextureUsage::RenderAttachment;
	depthTextureDesc.viewFormatCount = 1;
	depthTextureDesc.viewFormats = (WGPUTextureFormat*)&m_depthTextureFormat;
	m_depthTexture = m_device->createTexture(depthTextureDesc);

	// Create the view of the depth texture manipulated by the rasterizer
	TextureViewDescriptor depthTextureViewDesc;
	depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
	depthTextureViewDesc.baseArrayLayer = 0;
	depthTextureViewDesc.arrayLayerCount = 1;
	depthTextureViewDesc.baseMipLevel = 0;
	depthTextureViewDesc.mipLevelCount = 1;
	depthTextureViewDesc.dimension = TextureViewDimension::_2D;
	depthTextureViewDesc.format = m_depthTextureFormat;
	m_depthTextureView = m_depthTexture->createView(depthTextureViewDesc);

	return m_depthTextureView;
}

void Application::terminateDepthBuffer() {
	m_depthTexture->destroy();
}

bool Application::initRenderPipelines() {
	std::cout << "Creating shader module..." << std::endl;
	m_shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/shaders/shader.wgsl", *m_device);
	if (!m_shaderModule) {
		std::cerr << "Could not load shader from path!" << std::endl;
	}
	std::cout << "Shader module: " << m_shaderModule << std::endl;

	std::cout << "Creating render pipeline..." << std::endl;
	RenderPipelineDescriptor pipelineDesc;

	pipelineDesc.vertex.module = *m_shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;

	pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
	pipelineDesc.primitive.frontFace = FrontFace::CCW;
	pipelineDesc.primitive.cullMode = CullMode::None;

	FragmentState fragmentState;
	pipelineDesc.fragment = &fragmentState;
	fragmentState.module = *m_shaderModule;
	fragmentState.entryPoint = "fs_main";
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;

	BlendState blendState;
	blendState.color.srcFactor = BlendFactor::SrcAlpha;
	blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
	blendState.color.operation = BlendOperation::Add;
	blendState.alpha.srcFactor = BlendFactor::Zero;
	blendState.alpha.dstFactor = BlendFactor::One;
	blendState.alpha.operation = BlendOperation::Add;

	ColorTargetState colorTarget;
	colorTarget.format = m_surfaceFormat;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = ColorWriteMask::All;

	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;

	DepthStencilState depthStencilState = Default;
	depthStencilState.depthCompare = CompareFunction::Less;
	depthStencilState.depthWriteEnabled = true;
	depthStencilState.format = m_depthTextureFormat;
	depthStencilState.stencilReadMask = 0;
	depthStencilState.stencilWriteMask = 0;

	pipelineDesc.depthStencil = &depthStencilState;

	pipelineDesc.multisample.count = 1;
	pipelineDesc.multisample.mask = ~0u;
	pipelineDesc.multisample.alphaToCoverageEnabled = false;

	std::vector<BindGroupLayout> bindGroupLayouts = {
		*m_bindGroupLayout,
		*m_materialBindGroupLayout,
		*m_nodeBindGroupLayout
	};

	// Create the pipeline layout
	PipelineLayoutDescriptor layoutDesc{};
	layoutDesc.bindGroupLayoutCount = static_cast<uint32_t>(bindGroupLayouts.size());
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)bindGroupLayouts.data();
	PipelineLayout layout = m_device->createPipelineLayout(layoutDesc);
	pipelineDesc.layout = layout;


	for (uint32_t pipelineIdx = 0; pipelineIdx < m_gpuScene.renderPipelineCount(); ++pipelineIdx) {
		std::vector<VertexBufferLayout> vertexBufferLayouts = m_gpuScene.vertexBufferLayouts(pipelineIdx);
		pipelineDesc.vertex.bufferCount = static_cast<uint32_t>(vertexBufferLayouts.size());
		pipelineDesc.vertex.buffers = vertexBufferLayouts.data();
		pipelineDesc.primitive.topology = m_gpuScene.primitiveTopology(pipelineIdx);

		RenderPipeline pipeline = m_device->createRenderPipeline(pipelineDesc);
		std::cout << "Render pipeline: " << pipeline << std::endl;
		if (pipeline == nullptr) return false;
		m_pipelines.push_back(pipeline);
	}

	return true;
}

void Application::terminateRenderPipelines() {
	m_pipelines.clear();
}

bool Application::initGeometry(const ResourceManager::path& filePath) {
	auto extension = filePath.extension();
	bool success = false;

	if (extension == ".glb" || extension == ".gltf") {
		std::cout << "loading glTF file" << filePath << std::endl;
		success = ResourceManager::loadGeometryFromGltf(filePath, m_cpuScene);
		std::cout << "Creating scene from glTF..." << std::endl;
		m_gpuScene.createFromModel(m_device, m_cpuScene, *m_materialBindGroupLayout, *m_nodeBindGroupLayout);
		m_cpuScene = {};
	}
	else if (extension == ".obj") {
		std::cout << "loading OBJ file" << filePath << std::endl;
		std::cout << "Creating scene from OBJ..." << std::endl;
		// success = /* TBD Generate glTF from OBJ */
	}

	if (!success) {
		std::cerr << "Could not load geometry!" << std::endl;
	}
	return success;
}

bool Application::updateGeometry() {
	terminateRenderPipelines();
	if (initGeometry(m_filePath) && initRenderPipelines()) {
		return true;
	}
	return false;
}

void Application::terminateGeometry() {
	m_gpuScene.destroy();
	m_cpuScene = {};
}

bool Application::initUniforms() {
	// Create uniform buffer
	BufferDescriptor bufferDesc;
	bufferDesc.size = sizeof(GlobalUniforms);
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	m_uniformBuffer = m_device->createBuffer(bufferDesc);

	// Upload the initial value of the uniforms
	m_uniforms.modelMatrix = mat4x4(1.0);
	m_uniforms.viewMatrix = glm::lookAt(vec3(-2.0f, -3.0f, 2.0f), vec3(0.0f), vec3(0, 0, 1));
	m_uniforms.projectionMatrix = glm::perspective(45 * PI / 180, 640.0f / 480.0f, 0.01f, 100.0f);
	m_uniforms.time = 1.0f;
	m_uniforms.gamma = textureFormatGamma(m_surfaceFormat);
	float grey = std::pow(0.25, textureFormatGamma(m_surfaceFormat));
	m_uniforms.worldColor = { grey, grey,grey, 1.0 };
	m_queue->writeBuffer(*m_uniformBuffer, 0, &m_uniforms, sizeof(GlobalUniforms));

	updateProjectionMatrix();
	updateViewMatrix();

	return m_uniformBuffer;
}

void Application::terminateUniforms() {
	m_uniformBuffer->destroy();
}

bool Application::initLightingUniforms() {
	// Create uniform buffer
	BufferDescriptor bufferDesc;
	bufferDesc.size = sizeof(LightingUniforms);
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	m_lightingUniformBuffer = m_device->createBuffer(bufferDesc);

	// Initial values
	m_lightingUniforms.directions[0] = { 0.5f, -0.9f, 0.1f, 0.0f };
	m_lightingUniforms.directions[1] = { 0.2f, 0.4f, 0.3f, 0.0f };
	m_lightingUniforms.colors[0] = { 1.0f, 0.9f, 0.6f, 1.0f };
	m_lightingUniforms.colors[1] = { 0.6f, 0.9f, 1.0f, 1.0f };

	updateLightingUniforms();

	return m_lightingUniformBuffer;
}

void Application::terminateLightingUniforms() {
	m_lightingUniformBuffer->destroy();
}

void Application::updateLightingUniforms() {
	if (m_lightingUniformsChanged) {
		m_queue->writeBuffer(*m_lightingUniformBuffer, 0, &m_lightingUniforms, sizeof(LightingUniforms));
		m_lightingUniformsChanged = false;
	}
}

bool Application::initBindGroupLayouts() {
	{
		std::vector<BindGroupLayoutEntry> bindGroupLayoutEntries(2, Default);

		// The uniform buffer binding
		BindGroupLayoutEntry& bindingLayout = bindGroupLayoutEntries[0];
		bindingLayout.binding = 0;
		bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
		bindingLayout.buffer.type = BufferBindingType::Uniform;
		bindingLayout.buffer.minBindingSize = sizeof(GlobalUniforms);

		// The lighting uniform buffer binding
		BindGroupLayoutEntry& lightingUniformLayout = bindGroupLayoutEntries[1];
		lightingUniformLayout.binding = 1;
		lightingUniformLayout.visibility = ShaderStage::Fragment; // only Fragment is needed
		lightingUniformLayout.buffer.type = BufferBindingType::Uniform;
		lightingUniformLayout.buffer.minBindingSize = sizeof(LightingUniforms);

		// Create a bind group layout
		BindGroupLayoutDescriptor bindGroupLayoutDesc{};
		bindGroupLayoutDesc.entryCount = (uint32_t)bindGroupLayoutEntries.size();
		bindGroupLayoutDesc.entries = bindGroupLayoutEntries.data();
		m_bindGroupLayout = m_device->createBindGroupLayout(bindGroupLayoutDesc);
	}

	// Material bind group
	{
		std::vector<BindGroupLayoutEntry> bindGroupLayoutEntries(7, Default);
		// Material uniforms
		bindGroupLayoutEntries[0].binding = 0;
		bindGroupLayoutEntries[0].visibility = ShaderStage::Fragment;
		bindGroupLayoutEntries[0].buffer.type = BufferBindingType::Uniform;
		bindGroupLayoutEntries[0].buffer.minBindingSize = sizeof(GpuScene::MaterialUniforms);

		// Base color texture
		bindGroupLayoutEntries[1].binding = 1;
		bindGroupLayoutEntries[1].visibility = ShaderStage::Fragment;
		bindGroupLayoutEntries[1].texture.sampleType = TextureSampleType::Float;
		bindGroupLayoutEntries[1].texture.viewDimension = TextureViewDimension::_2D;

		// Base color sampler
		bindGroupLayoutEntries[2].binding = 2;
		bindGroupLayoutEntries[2].visibility = ShaderStage::Fragment;
		bindGroupLayoutEntries[2].sampler.type = SamplerBindingType::Filtering;

		// Metallic roughness texture
		bindGroupLayoutEntries[3].binding = 3;
		bindGroupLayoutEntries[3].visibility = ShaderStage::Fragment;
		bindGroupLayoutEntries[3].texture.sampleType = TextureSampleType::Float;
		bindGroupLayoutEntries[3].texture.viewDimension = TextureViewDimension::_2D;

		// Metallic roughness sampler
		bindGroupLayoutEntries[4].binding = 4;
		bindGroupLayoutEntries[4].visibility = ShaderStage::Fragment;
		bindGroupLayoutEntries[4].sampler.type = SamplerBindingType::Filtering;

		// Normal texture
		bindGroupLayoutEntries[5].binding = 5;
		bindGroupLayoutEntries[5].visibility = ShaderStage::Fragment;
		bindGroupLayoutEntries[5].texture.sampleType = TextureSampleType::Float;
		bindGroupLayoutEntries[5].texture.viewDimension = TextureViewDimension::_2D;

		// Normal sampler
		bindGroupLayoutEntries[6].binding = 6;
		bindGroupLayoutEntries[6].visibility = ShaderStage::Fragment;
		bindGroupLayoutEntries[6].sampler.type = SamplerBindingType::Filtering;

		BindGroupLayoutDescriptor bindGroupLayoutDesc{};
		bindGroupLayoutDesc.label = "Material";
		bindGroupLayoutDesc.entryCount = (uint32_t)bindGroupLayoutEntries.size();
		bindGroupLayoutDesc.entries = bindGroupLayoutEntries.data();
		m_materialBindGroupLayout = m_device->createBindGroupLayout(bindGroupLayoutDesc);
	}

	// Material bind group
	{
		std::vector<BindGroupLayoutEntry> bindGroupLayoutEntries(1, Default);
		// Material uniforms
		bindGroupLayoutEntries[0].binding = 0;
		bindGroupLayoutEntries[0].visibility = ShaderStage::Vertex;
		bindGroupLayoutEntries[0].buffer.type = BufferBindingType::Uniform;
		bindGroupLayoutEntries[0].buffer.minBindingSize = sizeof(GpuScene::NodeUniforms);

		BindGroupLayoutDescriptor bindGroupLayoutDesc{};
		bindGroupLayoutDesc.label = "Node";
		bindGroupLayoutDesc.entryCount = (uint32_t)bindGroupLayoutEntries.size();
		bindGroupLayoutDesc.entries = bindGroupLayoutEntries.data();
		m_nodeBindGroupLayout = m_device->createBindGroupLayout(bindGroupLayoutDesc);
	}

	return (
		m_bindGroupLayout &&
		m_materialBindGroupLayout &&
		m_nodeBindGroupLayout
		);
}

bool Application::initBindGroup() {
	// Create a binding
	std::vector<BindGroupEntry> bindings(2);

	bindings[0].binding = 0;
	bindings[0].buffer = *m_uniformBuffer;
	bindings[0].offset = 0;
	bindings[0].size = sizeof(GlobalUniforms);

	bindings[1].binding = 1;
	bindings[1].buffer = *m_lightingUniformBuffer;
	bindings[1].offset = 0;
	bindings[1].size = sizeof(LightingUniforms);

	BindGroupDescriptor bindGroupDesc;
	bindGroupDesc.layout = *m_bindGroupLayout;
	bindGroupDesc.entryCount = (uint32_t)bindings.size();
	bindGroupDesc.entries = bindings.data();
	m_bindGroup = m_device->createBindGroup(bindGroupDesc);

	return m_bindGroup;
}

void Application::updateProjectionMatrix() {
	// Update projection matrix
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);
	float ratio = width / (float)height;
	m_uniforms.projectionMatrix = glm::perspective(45 * PI / 180, ratio, 0.01f, 100.0f);
	m_queue->writeBuffer(
		*m_uniformBuffer,
		offsetof(GlobalUniforms, projectionMatrix),
		&m_uniforms.projectionMatrix,
		sizeof(GlobalUniforms::projectionMatrix)
	);
}

void Application::updateViewMatrix() {
	float cX = cos(m_cameraState.angles.x);
	float sX = sin(m_cameraState.angles.x);
	float cY = cos(m_cameraState.angles.y);
	float sY = sin(m_cameraState.angles.y);
	vec3 position = vec3(cX * cY, sX * cY, sY) * std::exp(-m_cameraState.zoom);
	mat4x4 translationMatrix = glm::translate(mat4x4(1.0f), -vec3(m_cameraState.pan.x, m_cameraState.pan.y, 0.0f));
	m_uniforms.viewMatrix = glm::lookAt(position, vec3(0.0), vec3(0, 0, 1));
	m_uniforms.viewMatrix = translationMatrix * m_uniforms.viewMatrix;
	m_queue->writeBuffer(
		*m_uniformBuffer,
		offsetof(GlobalUniforms, viewMatrix),
		&m_uniforms.viewMatrix,
		sizeof(GlobalUniforms::viewMatrix)
	);

	m_uniforms.cameraWorldPosition = position;
	m_queue->writeBuffer(
		*m_uniformBuffer,
		offsetof(GlobalUniforms, cameraWorldPosition),
		&m_uniforms.cameraWorldPosition,
		sizeof(GlobalUniforms::cameraWorldPosition)
	);
}

TextureView Application::getNextSurfaceTextureView()
{
	SurfaceTexture surfaceTexture;
	m_surface->getCurrentTexture(&surfaceTexture);

	if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success)
	{
		return nullptr;
	}

	Texture texture = surfaceTexture.texture;

	TextureViewDescriptor viewDescriptor;
	viewDescriptor.label = "Surface texture view";
	viewDescriptor.format = wgpuTextureGetFormat(surfaceTexture.texture);
	viewDescriptor.dimension = WGPUTextureViewDimension_2D;
	viewDescriptor.baseMipLevel = 0;
	viewDescriptor.mipLevelCount = 1;
	viewDescriptor.baseArrayLayer = 0;
	viewDescriptor.arrayLayerCount = 1;
	viewDescriptor.aspect = TextureAspect::All;
	TextureView targetView = texture.createView(viewDescriptor);

	return targetView;
}