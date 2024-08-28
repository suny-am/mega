#define WEBGPU_CPP_IMPLEMENTATION
// WGPU C++ wrapper
#include "webgpu/webgpu.hpp"
// GLM
#include "glm/glm.hpp"
#include <glm/ext.hpp>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif // __EMSCRIPTEN__


#include <array>
#include <iostream>
#include <cassert>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <cstddef>

using namespace wgpu;
namespace fs = std::filesystem;

using glm::mat4x4;
using glm::vec4;
using glm::vec3;

struct SharedUniforms {
	mat4x4 projectionMatrix;
	mat4x4 viewMatrix;
	mat4x4 modelMatrix;
	vec4 color;
	float time;
	float _pad[3];
};
// Calidate byte alignment in compile time
static_assert(sizeof(SharedUniforms) % 16 == 0);

class Application
{
public:
	bool Initialize();

	void Terminate();

	void MainLoop();

	bool IsRunning();

private:
	TextureView GetNextSurfaceTextureView();
	void InitializePipeline();
	RequiredLimits GetRequiredLimits(Adapter adapter) const;
	void InitializeBuffers();

private:
	GLFWwindow* window;
	Device device;
	Queue queue;
	Surface surface;
	std::unique_ptr<ErrorCallback> uncapturedErrorCallbackHandle;
	TextureFormat surfaceFormat = TextureFormat::Undefined;
	RenderPipeline renderPipeline;
	Buffer pointBuffer;
	Buffer indexBuffer;
	Buffer uniformBuffer;
	uint32_t indexCount;
	std::vector<float> pointData;
	std::vector<uint16_t> indexData;
	BindGroup bindGroup;
	SharedUniforms uniforms;
	TextureView depthTextureView;
	Texture depthTexture;
	mat4x4 S;
	mat4x4 T1;
	mat4x4 T2;
	mat4x4 R1;
	mat4x4 R2;
};


bool loadGeometry(const fs::path& path, std::vector<float>& pointData, std::vector<uint16_t>& indexData, int dimensions) {
	std::ifstream file(path);
	if (!file.is_open()) {
		return false;
	}

	pointData.clear();
	indexData.clear();

	enum class  Section {
		None,
		Points,
		Indicies
	};
	Section currentSection = Section::None;

	float value;
	uint16_t index;
	std::string line;
	while (!file.eof()) {
		getline(file, line);

		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}

		if (line == "[points]") {
			currentSection = Section::Points;
		}
		else if (line == "[indices]") {
			currentSection = Section::Indicies;
		}
		else if (line[0] == '#' || line.empty()) {

		}
		else if (currentSection == Section::Points) {
			std::istringstream iss(line);

			for (int i = 0; i < dimensions + 3; ++i) {
				iss >> value;
				pointData.push_back(value);
			}
		}
		else if (currentSection == Section::Indicies) {
			std::istringstream iss(line);

			for (int i = 0; i < 3; ++i) {
				iss >> index;
				indexData.push_back(index);
			}
		}
	}
	return true;
}

ShaderModule loadShaderModule(const fs::path& path, Device device) {
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
	shaderDesc.hintCount = 0;
	shaderDesc.hints = nullptr;
	shaderDesc.nextInChain = &shaderCodeDesc.chain;
	return device.createShaderModule(shaderDesc);
}

int main()
{
	Application app;

	if (!app.Initialize())
	{
		return 1;
	}

#ifdef __EMSCRIPTEN__
	auto callback = [](void* arg)
		{
			Application* pApp = reinterpret_cast<Application*>(arg);
			pApp->MainLoop();
		};
	emscripten_set_main_loop_arg(callback, &app, 0, true);
#else  // __EMSCRIPTEN__
	while (app.IsRunning())
	{
		app.MainLoop();
	}
#endif // __EMSCRIPTEN__

	return 0;
}

bool Application::Initialize()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);

	Instance instance = wgpuCreateInstance(nullptr);

	std::cout << "Requesting adapter..." << std::endl;
	surface = glfwGetWGPUSurface(instance, window);
	RequestAdapterOptions adapterOpts = {};
	adapterOpts.compatibleSurface = surface;
	Adapter adapter = instance.requestAdapter(adapterOpts);
	std::cout << "Got adapter: " << adapter << std::endl;

	instance.release();

	std::cout << "Requesting device..." << std::endl;
	DeviceDescriptor deviceDesc = {};
	deviceDesc.label = "My Device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = nullptr;
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "The default queue";
	deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message, void* /* pUserData */)
		{
			std::cout << "Device lost: reason " << reason;
			if (message)
				std::cout << " (" << message << ")";
			std::cout << std::endl;
		};
	RequiredLimits requiredLimits = GetRequiredLimits(adapter);
	deviceDesc.requiredLimits = &requiredLimits;
	device = adapter.requestDevice(deviceDesc);
	std::cout << "Got device: " << device << std::endl;

	uncapturedErrorCallbackHandle = device.setUncapturedErrorCallback([](ErrorType type, char const* message)
																	  {
																		  std::cout << "Uncaptured device error: type " << type;
																		  if (message) std::cout << " (" << message << ")";
																		  std::cout << std::endl; });

	queue = device.getQueue();

	SurfaceConfiguration surfaceConfig = {};

	surfaceConfig.width = 640;
	surfaceConfig.height = 480;
	surfaceConfig.usage = TextureUsage::RenderAttachment;
	surfaceFormat = surface.getPreferredFormat(adapter);
	surfaceConfig.format = surfaceFormat;
	surfaceConfig.viewFormatCount = 0;
	surfaceConfig.viewFormats = nullptr;
	surfaceConfig.device = device;
	surfaceConfig.presentMode = PresentMode::Fifo;
	surfaceConfig.alphaMode = CompositeAlphaMode::Auto;

	surface.configure(surfaceConfig);

	adapter.release();

	InitializeBuffers();
	InitializePipeline();

	return true;
}

void Application::Terminate()
{
	depthTextureView.release();
	depthTexture.destroy();
	depthTexture.release();
	pointBuffer.release();
	indexBuffer.release();
	renderPipeline.release();
	surface.unconfigure();
	queue.release();
	surface.release();
	device.release();
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Application::MainLoop()
{
	glfwPollEvents();

	TextureView targetView = GetNextSurfaceTextureView();
	if (!targetView)
		return;

	uniforms.time = static_cast<float>(glfwGetTime());
	queue.writeBuffer(uniformBuffer, offsetof(SharedUniforms, time), &uniforms.time, sizeof(SharedUniforms::time));

	float angle1 = uniforms.time;
	R1 = glm::rotate(mat4x4(1.0), angle1, vec3(0.0, 0.0, 1.0));
	uniforms.modelMatrix = R1 * T1 * S;
	queue.writeBuffer(uniformBuffer, offsetof(SharedUniforms, modelMatrix), &uniforms.modelMatrix, sizeof(SharedUniforms::modelMatrix));

	CommandEncoderDescriptor encoderDesc = {};
	encoderDesc.label = "My command encoder";
	CommandEncoder encoder = device.createCommandEncoder(encoderDesc);

	RenderPassDescriptor renderPassDesc = {};

	RenderPassColorAttachment renderPassColorAttachment = {};
	renderPassColorAttachment.view = targetView;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = LoadOp::Clear;
	renderPassColorAttachment.storeOp = StoreOp::Store;
	renderPassColorAttachment.clearValue = Color{ 0.05, 0.05, 0.05, 1.0 };
#ifndef WEBGPU_BACKEND_WGPU
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;

	RenderPassDepthStencilAttachment depthStencilAttachment;

	// 1.0f == default value for the depth buffer (furthest value)
	depthStencilAttachment.view = depthTextureView;
	depthStencilAttachment.depthClearValue = 1.0f;
	// Comparable settings to Color Attachments
	depthStencilAttachment.depthLoadOp = LoadOp::Clear;
	depthStencilAttachment.depthStoreOp = StoreOp::Store;
	// Possible to disable writing to depth buffer globally via this attribute
	depthStencilAttachment.depthReadOnly = false;

	// Not used but required
	depthStencilAttachment.stencilClearValue = 0;
	depthStencilAttachment.stencilReadOnly = false;
#ifdef WEBGPU_BACKEND_DAWN
	constexpr auto NanNf = std::nunmeric_limits<float>::quiet_NaN();
	depthStencilAttachment.clearDepth = NaNf;
	depthStencilAttachment.stencilLoadOp = LoadOp::Undefined;
	depthStencilAttachment.stencilStoreOp = StoreOp::Undefined;
#else 
	depthStencilAttachment.stencilLoadOp = LoadOp::Clear;
	depthStencilAttachment.stencilStoreOp = StoreOp::Store;
#endif
	depthStencilAttachment.stencilLoadOp = LoadOp::Clear;
	depthStencilAttachment.stencilStoreOp = StoreOp::Store;


	renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
	renderPassDesc.timestampWrites = nullptr;

	RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);

	renderPass.setPipeline(renderPipeline);

	renderPass.setVertexBuffer(0, pointBuffer, 0, pointData.size() * sizeof(float));
	renderPass.setIndexBuffer(indexBuffer, IndexFormat::Uint16, 0, indexData.size() * sizeof(uint16_t));
	renderPass.setBindGroup(0, bindGroup, 0, nullptr);

	renderPass.drawIndexed(indexCount, 1, 0, 0, 0);

	renderPass.end();
	renderPass.release();

	CommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.label = "Command buffer";
	CommandBuffer command = encoder.finish(cmdBufferDescriptor);
	encoder.release();

	std::cout << "Submitting command..." << std::endl;
	queue.submit(1, &command);
	command.release();
	std::cout << "Command submitted." << std::endl;

	targetView.release();
#ifndef __EMSCRIPTEN__
	surface.present();
#endif // EMSCRIPTEN

#if defined(WEBGPU_BACKEND_DAWN)
	device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
	device.poll(false);
#endif
}

bool Application::IsRunning()
{
	return !glfwWindowShouldClose(window);
}

TextureView Application::GetNextSurfaceTextureView()
{
	SurfaceTexture surfaceTexture;
	surface.getCurrentTexture(&surfaceTexture);

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

void Application::InitializePipeline()
{
	std::cout << "Creating shader module..." << std::endl;
	ShaderModule shaderModule = loadShaderModule(RESOURCE_DIR "/shader.wgsl", device);
	std::cout << "Shader module: " << shaderModule << std::endl;

	RenderPipelineDescriptor renderPipelineDesc;

	VertexBufferLayout vertexBufferLayout;
	std::vector<VertexAttribute> vertexAttribs(2);

	vertexAttribs[0].shaderLocation = 0;
	vertexAttribs[0].format = VertexFormat::Float32x3; // 3 dimensions
	vertexAttribs[0].offset = 0;

	vertexAttribs[1].shaderLocation = 1;
	vertexAttribs[1].format = VertexFormat::Float32x3;
	vertexAttribs[1].offset = 3 * sizeof(float); // 3 dimensions

	vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
	vertexBufferLayout.attributes = vertexAttribs.data();

	vertexBufferLayout.arrayStride = 6 * sizeof(float);
	vertexBufferLayout.stepMode = VertexStepMode::Vertex;

	renderPipelineDesc.vertex.buffers = &vertexBufferLayout;
	renderPipelineDesc.vertex.bufferCount = 1;

	renderPipelineDesc.vertex.module = shaderModule;
	renderPipelineDesc.vertex.entryPoint = "vs_main";
	renderPipelineDesc.vertex.constantCount = 0;
	renderPipelineDesc.vertex.constants = nullptr;

	renderPipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
	renderPipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
	renderPipelineDesc.primitive.frontFace = FrontFace::CCW;
	renderPipelineDesc.primitive.cullMode = CullMode::None;

	FragmentState fragmentState;
	fragmentState.module = shaderModule;
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

	ColorTargetState colorTargetState;
	colorTargetState.format = surfaceFormat;
	colorTargetState.blend = &blendState;
	colorTargetState.writeMask = ColorWriteMask::All;

	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTargetState;
	renderPipelineDesc.fragment = &fragmentState;

	DepthStencilState depthStencilState = Default;
	depthStencilState.depthCompare = CompareFunction::Less;
	depthStencilState.depthWriteEnabled = true;
	TextureFormat depthTextureFormat = TextureFormat::Depth24Plus;
	depthStencilState.format = depthTextureFormat;
	depthStencilState.stencilReadMask = 0;
	depthStencilState.stencilWriteMask = 0;

	TextureDescriptor depthTextureDesc;
	depthTextureDesc.dimension = TextureDimension::_2D;
	depthTextureDesc.format = depthTextureFormat;
	depthTextureDesc.mipLevelCount = 1;
	depthTextureDesc.sampleCount = 1;
	depthTextureDesc.size = { 640, 480, 1 };
	depthTextureDesc.usage = TextureUsage::RenderAttachment;
	depthTextureDesc.viewFormatCount = 1;
	depthTextureDesc.viewFormats = (WGPUTextureFormat*)&depthTextureFormat;
	depthTexture = device.createTexture(depthTextureDesc);

	TextureViewDescriptor depthTextureViewDesc;
	depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
	depthTextureViewDesc.baseArrayLayer = 0;
	depthTextureViewDesc.arrayLayerCount = 1;
	depthTextureViewDesc.mipLevelCount = 1;
	depthTextureViewDesc.dimension = TextureViewDimension::_2D;
	depthTextureViewDesc.format = depthTextureFormat;
	depthTextureView = depthTexture.createView(depthTextureViewDesc);


	renderPipelineDesc.depthStencil = &depthStencilState;

	renderPipelineDesc.multisample.count = 1;
	renderPipelineDesc.multisample.mask = ~0u;
	renderPipelineDesc.multisample.alphaToCoverageEnabled = false;

	BindGroupLayoutEntry bindingLayout = Default;
	bindingLayout.binding = 0;
	bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
	bindingLayout.buffer.type = BufferBindingType::Uniform;
	bindingLayout.buffer.minBindingSize = sizeof(SharedUniforms);

	BindGroupLayoutDescriptor bindGroupLayoutDesc;
	bindGroupLayoutDesc.entryCount = 1;
	bindGroupLayoutDesc.entries = &bindingLayout;
	BindGroupLayout bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutDesc);

	PipelineLayoutDescriptor layoutDesc;
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
	PipelineLayout layout = device.createPipelineLayout(layoutDesc);

	renderPipelineDesc.layout = layout;

	renderPipeline = device.createRenderPipeline(renderPipelineDesc);

	BindGroupEntry binding;
	binding.binding = 0;
	binding.buffer = uniformBuffer;
	binding.offset = 0;
	binding.size = sizeof(SharedUniforms);

	BindGroupDescriptor bindGroupDesc;
	bindGroupDesc.layout = bindGroupLayout;
	bindGroupDesc.entryCount = bindGroupLayoutDesc.entryCount;
	bindGroupDesc.entries = &binding;
	bindGroup = device.createBindGroup(bindGroupDesc);

	shaderModule.release();
}

RequiredLimits Application::GetRequiredLimits(Adapter adapter) const
{
	SupportedLimits supportedLimits;
	adapter.getLimits(&supportedLimits);

	RequiredLimits requiredLimits = Default;
	requiredLimits.limits.maxVertexAttributes = 2;
	requiredLimits.limits.maxVertexBuffers = 1;
	requiredLimits.limits.maxBufferSize = 21 * 5 * sizeof(float);
	requiredLimits.limits.maxVertexBufferArrayStride = 6 * sizeof(float);
	requiredLimits.limits.maxBindGroups = 1;
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
	requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	requiredLimits.limits.maxInterStageShaderComponents = 3;
	requiredLimits.limits.maxTextureDimension1D = 480;
	requiredLimits.limits.maxTextureDimension2D = 640;
	requiredLimits.limits.maxTextureArrayLayers = 1;

	return requiredLimits;
}

void Application::InitializeBuffers()
{
	bool success = loadGeometry(RESOURCE_DIR "/pyramid.txt", pointData, indexData, 3);
	if (!success) {
		std::cerr << "Could not load geometry!" << std::endl;
	}

	BufferDescriptor bufferDesc;

	bufferDesc.label = "Position Data";
	bufferDesc.size = pointData.size() * sizeof(float);
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
	bufferDesc.mappedAtCreation = false;
	pointBuffer = device.createBuffer(bufferDesc);
	queue.writeBuffer(pointBuffer, 0, pointData.data(), bufferDesc.size);

	indexCount = static_cast<int>(indexData.size());

	bufferDesc.label = "Index Data";
	bufferDesc.size = indexData.size() * sizeof(uint16_t);
	bufferDesc.size = (bufferDesc.size + 3) & ~3;
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Index;
	bufferDesc.mappedAtCreation = false;
	indexBuffer = device.createBuffer(bufferDesc);
	queue.writeBuffer(indexBuffer, 0, indexData.data(), bufferDesc.size);


	bufferDesc.label = "Uniform";
	bufferDesc.size = sizeof(SharedUniforms);
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	uniformBuffer = device.createBuffer(bufferDesc);

	uniforms.time = 1.0f;
	uniforms.color = { 0.0f, 1.0f, 0.4f, 1.0f };

	constexpr float PI = 3.14159265358979323846f;

	float angle1 = (float)glfwGetTime();
	float angle2 = 3.0 * PI / 4.0;
	vec3 focalPoint = vec3(0.0, 0.0, -2.0);

	S = glm::scale(mat4x4(1.0), vec3(0.3f));
	T1 = glm::translate(mat4x4(1.0), vec3(0.5, 0.0, 0.0));
	R1 = glm::rotate(mat4x4(1.0), angle1, vec3(0.5, 0.0, 0.0));
	uniforms.modelMatrix = R1 * T1 * S;

	R2 = glm::rotate(mat4x4(1.0), -angle2, vec3(1.0, 0.0, 0.0));
	T2 = glm::translate(mat4x4(1.0), -focalPoint);
	uniforms.viewMatrix = T2 * R2;

	float near = 0.001f;
	float far = 100.0f;
	float ratio = 640.0f / 480.0f;
	float focalLength = 2.0;
	float fov = 2 * glm::atan(1 / focalLength);
	uniforms.projectionMatrix = glm::perspective(fov, ratio, near, far);

	queue.writeBuffer(uniformBuffer, 0, &uniforms, sizeof(SharedUniforms));
}


