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

const char *shaderSource = R"(
@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
    var p = vec2f(0.0, 0.0);
    if in_vertex_index == 0u {
        p = vec2f(-0.5, -0.5);
    } else if (in_vertex_index == 1u) {
        p = vec2f(0.5, -0.5);
    } else {
        p = vec2f(0.0, 0.5);
    }
    return vec4f(p, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
    return vec4f(0.0, 0.4, 1.0, 1.0);
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

private:
	GLFWwindow *window;
	Device device;
	Queue queue;
	Surface surface;
	std::unique_ptr<ErrorCallback> uncapturedErrorCallbackHandle;
	TextureFormat surfaceFormat = TextureFormat::Undefined;
	RenderPipeline renderPipeline;
};

int main()
{
	Application app;

	if (!app.Initialize())
	{
		return 1;
	}

#ifdef __EMSCRIPTEN__
	auto callback = [](void *arg)
	{
		Application *pApp = reinterpret_cast<Application *>(arg);
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

	InstanceDescriptor desc = {};
	Instance instance = createInstance(desc);

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
	deviceDesc.defaultQueue.label = "The default queue";
	deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const *message, void * /* pUserData */)
	{
		std::cout << "Device lost: reason " << reason;
		if (message)
			std::cout << " (" << message << ")";
		std::cout << std::endl;
	};
	device = adapter.requestDevice(deviceDesc);
	std::cout << "Got device: " << device << std::endl;

	std::cout << "Adapter released." << std::endl;

	uncapturedErrorCallbackHandle = device.setUncapturedErrorCallback([](ErrorType type, char const *message)
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

	adapter.release();

	surfaceConfig.viewFormatCount = 0;
	surfaceConfig.viewFormats = nullptr;
	surfaceConfig.device = device;
	surfaceConfig.presentMode = PresentMode::Fifo;
	surfaceConfig.alphaMode = CompositeAlphaMode::Auto;

	surface.configure(surfaceConfig);

	InitializePipeline();

	return true;
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

	renderPipelineDesc.vertex.bufferCount = 0;
	renderPipelineDesc.vertex.buffers = nullptr;

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
	blendState.color.srcFactor = BlendFactor::Zero;
	blendState.color.dstFactor = BlendFactor::One;
	blendState.color.operation = BlendOperation::Add;

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

void Application::Terminate()
{
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
	renderPassColorAttachment.clearValue = WGPUColor{0.9, 0.1, 0.2, 1.0};
#ifndef WEBGPU_BACKEND_WGPU
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	renderPassDesc.depthStencilAttachment = nullptr;
	renderPassDesc.timestampWrites = nullptr;

	RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
	
	renderPass.setPipeline(renderPipeline);
	renderPass.draw(3, 1, 0, 0);
	
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
	viewDescriptor.aspect = WGPUTextureAspect_All;
	TextureView targetView = texture.createView(viewDescriptor);

	return targetView;
}