#include "application.h"
#include "resource-manager.h"
#include "webgpu-std-utils.hpp"

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm/glm.hpp>
#include <glm/glm/ext.hpp>
#include <glm/glm/gtx/polar_coordinates.hpp>

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

// Custom ImGui widgets
namespace ImGui {
	bool DragDirection(const char* label, glm::vec4& direction) {
		glm::vec2 angles = glm::degrees(glm::polar(glm::vec3(direction)));
		bool changed = ImGui::DragFloat2(label, glm::value_ptr(angles));
		direction = glm::vec4(glm::euclidean(glm::radians(angles)), direction.w);
		return changed;
	}
} // namespace ImGui

///////////////////////////////////////////////////////////////////////////////
// Public methods

bool Application::onInit() {
	if (!initWindowAndDevice()) return false;
	if (!initSurfaceConfiguration()) return false;
	if (!initDepthBuffer()) return false;
	if (!initBindGroupLayouts()) return false;
	if (!initGeometry()) return false;
	if (!initRenderPipelines()) return false;
	if (!initUniforms()) return false;
	if (!initLightingUniforms()) return false;
	if (!initBindGroup()) return false;
	if (!initGui()) return false;
	return true;
}

void Application::onFinish() {
	terminateGui();
	terminateBindGroup();
	terminateLightingUniforms();
	terminateUniforms();
	terminateRenderPipelines();
	terminateGeometry();
	terminateBindGroupLayouts();
	terminateDepthBuffer();
	terminateWindowAndDevice();
}

void Application::onFrame() {
	glfwPollEvents();
	updateDragInertia();
	updateLightingUniforms();

	// Update uniform buffer
	m_uniforms.time = static_cast<float>(glfwGetTime());
	m_queue.writeBuffer(m_uniformBuffer, offsetof(GlobalUniforms, time), &m_uniforms.time, sizeof(GlobalUniforms::time));

	TextureView nextTexture = getNextSurfaceTextureView();
	if (!nextTexture) {
		std::cerr << "Could not acquire next texture from surface configuration" << std::endl;
		return;
	}

	CommandEncoderDescriptor commandEncoderDesc;
	commandEncoderDesc.label = "Command Encoder";
	CommandEncoder encoder = m_device.createCommandEncoder(commandEncoderDesc);

	RenderPassDescriptor renderPassDesc{};

	RenderPassColorAttachment renderPassColorAttachment{};
	renderPassColorAttachment.view = nextTexture;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = LoadOp::Clear;
	renderPassColorAttachment.storeOp = StoreOp::Store;
	float grey = std::pow(0.25, textureFormatGamma(m_surfaceFormat));
	renderPassColorAttachment.clearValue = Color{ grey, grey, grey, 1.0 };
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;

	RenderPassDepthStencilAttachment depthStencilAttachment;
	depthStencilAttachment.view = m_depthTextureView;
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
		renderPass.setBindGroup(0, m_bindGroup, 0, nullptr);

		m_gpuScene.draw(renderPass, pipelineIdx);
	}

	// We add the GUI drawing commands to the render pass
	updateGui(renderPass);

	renderPass.end();
	renderPass.release();

	nextTexture.release();

	CommandBufferDescriptor cmdBufferDescriptor{};
	cmdBufferDescriptor.label = "Command buffer";
	CommandBuffer command = encoder.finish(cmdBufferDescriptor);
	encoder.release();

	m_queue.submit(command);
	command.release();

	m_surface.present();

#ifdef WEBGPU_BACKEND_DAWN
	// Check for pending error callbacks
	m_device.tick();
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

void Application::onMouseMove(double xpos, double ypos) {
	if (m_drag.active) {
		vec2 currentMouse = vec2(-(float)xpos, (float)ypos);
		vec2 delta = (currentMouse - m_drag.startMouse) * m_drag.sensitivity;
		m_cameraState.angles = m_drag.startCameraState.angles + delta;
		// Clamp to avoid going too far when orbitting up/down
		m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
		updateViewMatrix();

		// Inertia
		m_drag.velocity = delta - m_drag.previousDelta;
		m_drag.previousDelta = delta;
	}
}

void Application::onMouseButton(int button, int action, int /* modifiers */) {
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		// Don't rotate the camera if the mouse is already captured by an ImGui
		// interaction at this frame.
		return;
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		switch (action) {
		case GLFW_PRESS:
			m_drag.active = true;
			double xpos, ypos;
			glfwGetCursorPos(m_window, &xpos, &ypos);
			m_drag.startMouse = vec2(-(float)xpos, (float)ypos);
			m_drag.startCameraState = m_cameraState;
			break;
		case GLFW_RELEASE:
			m_drag.active = false;
			break;
		}
	}
}

void Application::onScroll(double /* xoffset */, double yoffset) {
	m_cameraState.zoom += m_drag.scrollSensitivity * static_cast<float>(yoffset);
	m_cameraState.zoom = glm::clamp(m_cameraState.zoom, -2.0f, 2.0f);
	updateViewMatrix();
}

///////////////////////////////////////////////////////////////////////////////
// Private methods

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

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_window = glfwCreateWindow(640, 480, "Learn WebGPU", NULL, NULL);
	if (!m_window) {
		std::cerr << "Could not open window!" << std::endl;
		return false;
	}

	std::cout << "Requesting adapter..." << std::endl;
	m_surface = glfwGetWGPUSurface(m_instance, m_window);
	RequestAdapterOptions adapterOpts{};
	adapterOpts.compatibleSurface = m_surface;
	Adapter adapter = m_instance.requestAdapter(adapterOpts);
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
	// Allow textures up to 2K
	requiredLimits.limits.maxTextureDimension1D = 2048;
	requiredLimits.limits.maxTextureDimension2D = 2048;
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
	m_errorCallbackHandle = m_device.setUncapturedErrorCallback([](ErrorType type, char const* message) {
		std::cout << "Device error: type " << type;
		if (message) std::cout << " (message: " << message << ")";
		std::cout << std::endl;
																});

	m_queue = m_device.getQueue();

#ifdef WEBGPU_BACKEND_WGPU
	m_surfaceFormat = m_surface.getPreferredFormat(adapter);
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
	glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onMouseMove(xpos, ypos);
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
	return m_device != nullptr;
}

void Application::terminateWindowAndDevice() {
	m_queue.release();
	m_device.release();
	m_surface.release();
	m_instance.release();

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
	surfaceConfig.device = m_device;
	surfaceConfig.presentMode = PresentMode::Fifo;
	surfaceConfig.alphaMode = CompositeAlphaMode::Auto;

	m_surface.configure(surfaceConfig);

	return m_surface != nullptr;
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
	m_depthTexture = m_device.createTexture(depthTextureDesc);
	std::cout << "Depth texture: " << m_depthTexture << std::endl;

	// Create the view of the depth texture manipulated by the rasterizer
	TextureViewDescriptor depthTextureViewDesc;
	depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
	depthTextureViewDesc.baseArrayLayer = 0;
	depthTextureViewDesc.arrayLayerCount = 1;
	depthTextureViewDesc.baseMipLevel = 0;
	depthTextureViewDesc.mipLevelCount = 1;
	depthTextureViewDesc.dimension = TextureViewDimension::_2D;
	depthTextureViewDesc.format = m_depthTextureFormat;
	m_depthTextureView = m_depthTexture.createView(depthTextureViewDesc);
	std::cout << "Depth texture view: " << m_depthTextureView << std::endl;

	return m_depthTextureView != nullptr;
}

void Application::terminateDepthBuffer() {
	m_depthTextureView.release();
	m_depthTexture.destroy();
	m_depthTexture.release();
}


bool Application::initRenderPipelines() {
	std::cout << "Creating shader module..." << std::endl;
	m_shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/shader.wgsl", m_device);
	std::cout << "Shader module: " << m_shaderModule << std::endl;

	std::cout << "Creating render pipeline..." << std::endl;
	RenderPipelineDescriptor pipelineDesc;

	pipelineDesc.vertex.module = m_shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;

	pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
	pipelineDesc.primitive.frontFace = FrontFace::CCW;
	pipelineDesc.primitive.cullMode = CullMode::None;

	FragmentState fragmentState;
	pipelineDesc.fragment = &fragmentState;
	fragmentState.module = m_shaderModule;
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
		m_bindGroupLayout,
		m_materialBindGroupLayout,
		m_nodeBindGroupLayout
	};

	// Create the pipeline layout
	PipelineLayoutDescriptor layoutDesc{};
	layoutDesc.bindGroupLayoutCount = static_cast<uint32_t>(bindGroupLayouts.size());
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)bindGroupLayouts.data();
	PipelineLayout layout = m_device.createPipelineLayout(layoutDesc);
	pipelineDesc.layout = layout;

	for (uint32_t pipelineIdx = 0; pipelineIdx < m_gpuScene.renderPipelineCount(); ++pipelineIdx) {
		// Vertex fetch
		std::vector<VertexBufferLayout> vertexBufferLayouts = m_gpuScene.vertexBufferLayouts(pipelineIdx);
		pipelineDesc.vertex.bufferCount = static_cast<uint32_t>(vertexBufferLayouts.size());
		pipelineDesc.vertex.buffers = vertexBufferLayouts.data();
		pipelineDesc.primitive.topology = m_gpuScene.primitiveTopology(pipelineIdx);

		RenderPipeline pipeline = m_device.createRenderPipeline(pipelineDesc);
		std::cout << "Render pipeline: " << pipeline << std::endl;
		if (pipeline == nullptr) return false;
		m_pipelines.push_back(pipeline);
	}

	return true;
}

void Application::terminateRenderPipelines() {
	for (RenderPipeline pipeline : m_pipelines) {
		pipeline.release();
	}
	m_pipelines.clear();
	m_shaderModule.release();
}


bool Application::initGeometry() {
	// Load mesh data from OBJ file
	// bool success = ResourceManager::loadGeometryFromGltf(RESOURCE_DIR "/scenes/BusterDrone.glb", m_cpuScene);
	bool success = ResourceManager::loadGeometryFromGltf(RESOURCE_DIR "/scenes/DamagedHelmet.glb", m_cpuScene);
	//bool success = ResourceManager::loadGeometryFromGltf(RESOURCE_DIR "/scenes/triangle.gltf", m_cpuScene);
	if (!success) {
		std::cerr << "Could not load geometry!" << std::endl;
		return false;
	}
	m_gpuScene.createFromModel(m_device, m_cpuScene, m_materialBindGroupLayout, m_nodeBindGroupLayout);

	return true;
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
	m_uniformBuffer = m_device.createBuffer(bufferDesc);

	// Upload the initial value of the uniforms
	m_uniforms.viewMatrix = glm::lookAt(vec3(-2.0f, -3.0f, 2.0f), vec3(0.0f), vec3(0, 0, 1));
	m_uniforms.projectionMatrix = glm::perspective(45 * PI / 180, 640.0f / 480.0f, 0.01f, 100.0f);
	m_uniforms.time = 1.0f;
	m_uniforms.gamma = textureFormatGamma(m_surfaceFormat);
	m_queue.writeBuffer(m_uniformBuffer, 0, &m_uniforms, sizeof(GlobalUniforms));

	updateProjectionMatrix();
	updateViewMatrix();

	return m_uniformBuffer != nullptr;
}

void Application::terminateUniforms() {
	m_uniformBuffer.destroy();
	m_uniformBuffer.release();
}


bool Application::initLightingUniforms() {
	// Create uniform buffer
	BufferDescriptor bufferDesc;
	bufferDesc.size = sizeof(LightingUniforms);
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	m_lightingUniformBuffer = m_device.createBuffer(bufferDesc);

	// Initial values
	m_lightingUniforms.directions[0] = { 0.5f, -0.9f, 0.1f, 0.0f };
	m_lightingUniforms.directions[1] = { 0.2f, 0.4f, 0.3f, 0.0f };
	m_lightingUniforms.colors[0] = { 1.0f, 0.9f, 0.6f, 1.0f };
	m_lightingUniforms.colors[1] = { 0.6f, 0.9f, 1.0f, 1.0f };

	updateLightingUniforms();

	return m_lightingUniformBuffer != nullptr;
}

void Application::terminateLightingUniforms() {
	m_lightingUniformBuffer.destroy();
	m_lightingUniformBuffer.release();
}

void Application::updateLightingUniforms() {
	if (m_lightingUniformsChanged) {
		m_queue.writeBuffer(m_lightingUniformBuffer, 0, &m_lightingUniforms, sizeof(LightingUniforms));
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
		m_bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);
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
		m_materialBindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);
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
		m_nodeBindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);
	}

	return (
		m_bindGroupLayout != nullptr &&
		m_materialBindGroupLayout != nullptr &&
		m_nodeBindGroupLayout != nullptr
		);
}

void Application::terminateBindGroupLayouts() {
	m_bindGroupLayout.release();
	m_materialBindGroupLayout.release();
}


bool Application::initBindGroup() {
	// Create a binding
	std::vector<BindGroupEntry> bindings(2);

	bindings[0].binding = 0;
	bindings[0].buffer = m_uniformBuffer;
	bindings[0].offset = 0;
	bindings[0].size = sizeof(GlobalUniforms);

	bindings[1].binding = 1;
	bindings[1].buffer = m_lightingUniformBuffer;
	bindings[1].offset = 0;
	bindings[1].size = sizeof(LightingUniforms);

	BindGroupDescriptor bindGroupDesc;
	bindGroupDesc.layout = m_bindGroupLayout;
	bindGroupDesc.entryCount = (uint32_t)bindings.size();
	bindGroupDesc.entries = bindings.data();
	m_bindGroup = m_device.createBindGroup(bindGroupDesc);

	return m_bindGroup != nullptr;
}

void Application::terminateBindGroup() {
	m_bindGroup.release();
}

void Application::updateProjectionMatrix() {
	// Update projection matrix
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);
	float ratio = width / (float)height;
	m_uniforms.projectionMatrix = glm::perspective(45 * PI / 180, ratio, 0.01f, 100.0f);
	m_queue.writeBuffer(
		m_uniformBuffer,
		offsetof(GlobalUniforms, projectionMatrix),
		&m_uniforms.projectionMatrix,
		sizeof(GlobalUniforms::projectionMatrix)
	);
}

void Application::updateViewMatrix() {
	float cx = cos(m_cameraState.angles.x);
	float sx = sin(m_cameraState.angles.x);
	float cy = cos(m_cameraState.angles.y);
	float sy = sin(m_cameraState.angles.y);
	vec3 position = vec3(cx * cy, sx * cy, sy) * std::exp(-m_cameraState.zoom);
	m_uniforms.viewMatrix = glm::lookAt(position, vec3(0.0f), vec3(0, 0, 1));
	m_queue.writeBuffer(
		m_uniformBuffer,
		offsetof(GlobalUniforms, viewMatrix),
		&m_uniforms.viewMatrix,
		sizeof(GlobalUniforms::viewMatrix)
	);

	m_uniforms.cameraWorldPosition = position;
	m_queue.writeBuffer(
		m_uniformBuffer,
		offsetof(GlobalUniforms, cameraWorldPosition),
		&m_uniforms.cameraWorldPosition,
		sizeof(GlobalUniforms::cameraWorldPosition)
	);
}

void Application::updateDragInertia() {
	constexpr float eps = 1e-4f;
	// Apply inertia only when the user released the click.
	if (!m_drag.active) {
		// Avoid updating the matrix when the velocity is no longer noticeable
		if (std::abs(m_drag.velocity.x) < eps && std::abs(m_drag.velocity.y) < eps) {
			return;
		}
		m_cameraState.angles += m_drag.velocity;
		m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
		// Dampen the velocity so that it decreases exponentially and stops
		// after a few frames.
		m_drag.velocity *= m_drag.intertia;
		updateViewMatrix();
	}
}

bool Application::initGui() {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOther(m_window, true);
	ImGui_ImplGlfw_InitForOther(m_window, true);
	ImGui_ImplWGPU_InitInfo initInfo;

	initInfo.Device = m_device;
	initInfo.NumFramesInFlight = 3;
	initInfo.RenderTargetFormat = m_surfaceFormat;
	initInfo.DepthStencilFormat = m_depthTextureFormat;

	ImGui_ImplWGPU_Init(&initInfo);
	return true;
}

void Application::terminateGui() {
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplWGPU_Shutdown();
}

void Application::updateGui(RenderPassEncoder renderPass) {
	// Start the Dear ImGui frame
	ImGui_ImplWGPU_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Build our UI
	{
		bool changed = false;
		ImGui::Begin("Lighting");
		changed = ImGui::ColorEdit3("Color #0", glm::value_ptr(m_lightingUniforms.colors[0])) || changed;
		changed = ImGui::DragDirection("Direction #0", m_lightingUniforms.directions[0]) || changed;
		changed = ImGui::ColorEdit3("Color #1", glm::value_ptr(m_lightingUniforms.colors[1])) || changed;
		changed = ImGui::DragDirection("Direction #1", m_lightingUniforms.directions[1]) || changed;
		ImGui::End();
		m_lightingUniformsChanged = changed;
	}

	// Draw the UI
	ImGui::EndFrame();
	// Convert the UI defined above into low-level drawing commands
	ImGui::Render();
	// Execute the low-level drawing commands on the WebGPU backend
	ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
}

TextureView Application::getNextSurfaceTextureView()
{
	SurfaceTexture surfaceTexture;
	m_surface.getCurrentTexture(&surfaceTexture);

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