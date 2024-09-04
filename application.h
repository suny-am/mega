#pragma once

#include "gpu-scene.h"
#include "resource-loaders/tiny_gltf.h"

#include <webgpu/webgpu.hpp>
#include <glm/glm/glm.hpp>

#include <array>

// Forward declare
struct GLFWwindow;

class Application {
public:
	// A function called only once at the beginning. Returns false if init failed.
	bool onInit();

	// A function called at each frame, guaranteed never to be called before `onInit`.
	void onFrame();

	// A function called only once at the very end.
	void onFinish();

	// A function that tells if the application is still running.
	bool isRunning();

	// A function called when the window is resized.
	void onResize();

	// Mouse events
	void onMouseMove(double xpos, double ypos);
	void onMouseButton(int button, int action, int mods);
	void onScroll(double xoffset, double yoffset);

private:
	bool initWindowAndDevice();
	void terminateWindowAndDevice();

	bool initSurfaceConfiguration();

	bool initDepthBuffer();
	void terminateDepthBuffer();

	bool initRenderPipelines();
	void terminateRenderPipelines();

	bool initGeometry();
	void terminateGeometry();

	bool initUniforms();
	void terminateUniforms();

	bool initLightingUniforms();
	void terminateLightingUniforms();
	void updateLightingUniforms();

	bool initBindGroupLayouts();
	void terminateBindGroupLayouts();

	bool initBindGroup();
	void terminateBindGroup();

	void updateProjectionMatrix();
	void updateViewMatrix();

	bool initGui();
	void terminateGui();
	void updateGui(wgpu::RenderPassEncoder renderPass);

	wgpu::TextureView getNextSurfaceTextureView();

public:
	using vec2 = glm::vec2;

	struct CameraState {
		// angles.x is the rotation of the camera around the global vertical axis, affected by mouse.x
		// angles.y is the rotation of the camera around its local horizontal axis, affected by mouse.y
		vec2 angles = { 0.8f, 0.5f };
		// zoom is the position of the camera along its local forward axis, affected by the scroll wheel
		float zoom = -1.2f;
	};

	struct DragState {
		// Whether a drag action is ongoing (i.e., we are between mouse press and mouse release)
		bool active = false;
		// The position of the mouse at the beginning of the drag action
		vec2 startPos;
		// The camera state at the beginning of the drag action
		CameraState startCameraState;

		// Constant settings
		float sensitivity = 0.01f;
		float scrollSensitivity = 0.1f;

		// Inertia
		vec2 velocity = { 0.0, 0.0 };
		vec2 previousDelta;
		float inertia = 0.9f;
	};
	CameraState m_cameraState;
	DragState m_drag;

private:
	using mat4x4 = glm::mat4x4;
	using vec4 = glm::vec4;
	using vec3 = glm::vec3;

	struct GlobalUniforms {
		mat4x4 projectionMatrix;
		mat4x4 viewMatrix;
		vec3 cameraWorldPosition;
		float time;
		float gamma;
		float _pad1[3];
	};
	static_assert(sizeof(GlobalUniforms) % 16 == 0);

	struct LightingUniforms {
		std::array<vec4, 2> directions;
		std::array<vec4, 2> colors;
	};
	static_assert(sizeof(LightingUniforms) % 16 == 0);


	// Window and Device
	GLFWwindow* m_window = nullptr;
	wgpu::Instance m_instance = nullptr;
	wgpu::Surface m_surface = nullptr;
	wgpu::Device m_device = nullptr;
	wgpu::Queue m_queue = nullptr;
	wgpu::TextureFormat m_surfaceFormat = wgpu::TextureFormat::Undefined;
	// Keep the error callback alive
	std::unique_ptr<wgpu::ErrorCallback> m_errorCallbackHandle;

	// Depth Buffer
	wgpu::TextureFormat m_depthTextureFormat = wgpu::TextureFormat::Depth24Plus;
	wgpu::Texture m_depthTexture = nullptr;
	wgpu::TextureView m_depthTextureView = nullptr;

	// Render Pipeline
	wgpu::ShaderModule m_shaderModule = nullptr;
	std::vector<wgpu::RenderPipeline> m_pipelines;

	// Texture
	wgpu::Sampler m_sampler = nullptr;
	wgpu::Texture m_texture = nullptr;
	wgpu::TextureView m_textureView = nullptr;

	// Geometry
	tinygltf::Model m_cpuScene;
	GpuScene m_gpuScene;

	// Uniforms
	wgpu::Buffer m_uniformBuffer = nullptr;
	GlobalUniforms m_uniforms;
	wgpu::Buffer m_lightingUniformBuffer = nullptr;
	LightingUniforms m_lightingUniforms;
	bool m_lightingUniformsChanged = true;

	// Bind Group Layout
	wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
	wgpu::BindGroupLayout m_materialBindGroupLayout = nullptr;
	wgpu::BindGroupLayout m_nodeBindGroupLayout = nullptr;

	// Bind Group
	wgpu::BindGroup m_bindGroup = nullptr;

};