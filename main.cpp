#define WEBGPU_CPP_IMPLEMENTATION
#include "webgpu/webgpu.hpp"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <iostream>
#include <cassert>
#include <vector>

using namespace wgpu;

const char* shaderSource = R"(
struct VertexInput 
{
	@location(0) position: vec2f,
	@location(1) color: vec3f,
};

struct VertexOutput 
{
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	out.position = vec4f(in.position, 0.0, 1.0);
	out.color = in.color; 
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	return vec4f(in.color, 1.0); 
}
)";


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
	Buffer positionBuffer;
	Buffer colorBuffer;
	uint32_t vertexCount;
};

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

	InitializePipeline();
	InitializeBuffers();

	return true;
}

void Application::Terminate()
{
	positionBuffer.release();
	colorBuffer.release();
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
	renderPassDesc.depthStencilAttachment = nullptr;
	renderPassDesc.timestampWrites = nullptr;

	RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);

	std::vector<VertexBufferLayout> vertexBufferLayouts(2);

	renderPass.setPipeline(renderPipeline);

	// Set vertex buffers while encoding the render pass
	renderPass.setVertexBuffer(0, positionBuffer, 0, positionBuffer.getSize());
	renderPass.setVertexBuffer(1, colorBuffer, 0, colorBuffer.getSize());
	//                         ^ Add a second call to set the second vertex buffer

	// We use the `vertexCount` variable instead of hard-coding the vertex count
	renderPass.draw(vertexCount, 1, 0, 0);

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
	ShaderModuleDescriptor shaderModuleDesc;
#ifdef WEBGPU_BACKEND_WGPU
	shaderModuleDesc.hintCount = 0;
	shaderModuleDesc.hints = nullptr;
#endif

	ShaderModuleWGSLDescriptor shaderCodeDesc;
	shaderCodeDesc.chain.next = nullptr;
	shaderCodeDesc.chain.sType = SType::ShaderModuleWGSLDescriptor;
	shaderModuleDesc.nextInChain = &shaderCodeDesc.chain;
	shaderCodeDesc.code = shaderSource;
	ShaderModule shaderModule = device.createShaderModule(shaderModuleDesc);

	RenderPipelineDescriptor renderPipelineDesc;

	std::vector<VertexBufferLayout> vertexBufferLayouts(2);

	// Position attribute remains untouched
	VertexAttribute positionAttrib;
	positionAttrib.shaderLocation = 0; // @location(0)
	positionAttrib.format = VertexFormat::Float32x2; // size of position
	positionAttrib.offset = 0;

	vertexBufferLayouts[0].attributeCount = 1;
	vertexBufferLayouts[0].attributes = &positionAttrib;
	vertexBufferLayouts[0].arrayStride = 2 * sizeof(float); // stride = size of position
	vertexBufferLayouts[0].stepMode = VertexStepMode::Vertex;

	// Color attribute
	VertexAttribute colorAttrib;
	colorAttrib.shaderLocation = 1; // @location(1)
	colorAttrib.format = VertexFormat::Float32x3; // size of color
	colorAttrib.offset = 0;

	vertexBufferLayouts[1].attributeCount = 1;
	vertexBufferLayouts[1].attributes = &colorAttrib;
	vertexBufferLayouts[1].arrayStride = 3 * sizeof(float); // stride = size of color
	vertexBufferLayouts[1].stepMode = VertexStepMode::Vertex;

	renderPipelineDesc.vertex.bufferCount = static_cast<uint32_t>(vertexBufferLayouts.size());
	renderPipelineDesc.vertex.buffers = vertexBufferLayouts.data();

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

	renderPipelineDesc.depthStencil = nullptr;

	renderPipelineDesc.multisample.count = 1;
	renderPipelineDesc.multisample.mask = ~0u;
	renderPipelineDesc.multisample.alphaToCoverageEnabled = false;

	renderPipelineDesc.layout = nullptr;

	renderPipeline = device.createRenderPipeline(renderPipelineDesc);

	shaderModule.release();
}

RequiredLimits Application::GetRequiredLimits(Adapter adapter) const
{
	SupportedLimits supportedLimits;
	adapter.getLimits(&supportedLimits);

	RequiredLimits requiredLimits = Default;

	requiredLimits.limits.maxVertexAttributes = 2;
	requiredLimits.limits.maxVertexBuffers = 2;
	requiredLimits.limits.maxBufferSize = 6 * 3 * sizeof(float);
	requiredLimits.limits.maxVertexBufferArrayStride = 3 * sizeof(float);
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	requiredLimits.limits.maxInterStageShaderComponents = 3;

	return requiredLimits;
}

void Application::InitializeBuffers()
{
	std::vector<float> positionData = {
		-0.5, -0.5,
		+0.5, -0.5,
		+0.0, +0.5,
		-0.55f, -0.5,
		-0.05f, +0.5,
		-0.55f, +0.5
	};

	std::vector<float> colorData = {
		1.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 0.0, 1.0,
		1.0, 1.0, 0.0,
		1.0, 0.0, 1.0,
		0.0, 1.0, 1.0
	};

	vertexCount = static_cast<uint32_t>(positionData.size() / 2);
	assert(vertexCount == static_cast<uint32_t>(colorData.size() / 3));

	// Create vertex buffers
	BufferDescriptor bufferDesc;
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
	bufferDesc.mappedAtCreation = false;

	bufferDesc.label = "Vertex Position";
	bufferDesc.size = positionData.size() * sizeof(float);
	positionBuffer = device.createBuffer(bufferDesc);
	queue.writeBuffer(positionBuffer, 0, positionData.data(), bufferDesc.size);

	bufferDesc.label = "Vertex Color";
	bufferDesc.size = colorData.size() * sizeof(float);
	colorBuffer = device.createBuffer(bufferDesc);
	queue.writeBuffer(colorBuffer, 0, colorData.data(), bufferDesc.size);
};