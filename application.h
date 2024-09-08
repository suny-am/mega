#pragma once

#include "gpu-scene.h"
#include "resource-manager.h"

#include "resource-loaders/tiny_gltf.h"

#include <webgpu/webgpu.hpp>
#include <webgpu/webgpu-raii.hpp>
#include <glm/glm/glm.hpp>

#include <array>

// Forward declare
struct GLFWwindow;

using VertexAttributes = ResourceManager::VertexAttributes;

using namespace wgpu;

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

	bool initGeometry(const ResourceManager::path& filePath);
	bool updateGeometry();
	void terminateGeometry();

	bool initUniforms();
	void terminateUniforms();

	bool initLightingUniforms();
	void terminateLightingUniforms();
	void updateLightingUniforms();

	bool initBindGroupLayouts();

	bool initBindGroup();

	void updateProjectionMatrix();
	void updateViewMatrix();

	TextureView getNextSurfaceTextureView();

public:
	using mat4x4 = glm::mat4x4;
	using vec4 = glm::vec4;
	using vec3 = glm::vec3;
	using vec2 = glm::vec2;

	enum MouseAction {
		Orbit,
		Zoom,
		Pan
	};

	struct GlobalUniforms {
		mat4x4 projectionMatrix;
		mat4x4 viewMatrix;
		mat4x4 modelMatrix;
		vec4 worldColor;
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

	struct CameraState {
		// angles.x is the rotation of the camera around the global vertical axis, affected by mouse.x
		// angles.y is the rotation of the camera around its local horizontal axis, affected by mouse.y
		vec2 angles = { 0.8f, 0.5f };
		// zoom is the position of the camera along its local forward axis, affected by the scroll wheel
		float zoom = -1.2f;
		// pan.x is the translation of the camera along the global vertical axis, affected by mouse.x
		// pan.y is the translation of the camera along the local horizonal axis, affected by mouse.y
		vec2 pan = { 0.0f, 0.0f };
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
		MouseAction mouseAction = MouseAction::Orbit;
	};

	GlobalUniforms m_uniforms;
	LightingUniforms m_lightingUniforms;

	CameraState m_cameraState;
	DragState m_drag;

private:
	GLFWwindow* m_window = nullptr;
	raii::Instance m_instance;
	raii::Surface m_surface;
	raii::Device m_device;
	raii::Queue m_queue;
	TextureFormat m_surfaceFormat = TextureFormat::Undefined;
	std::unique_ptr<ErrorCallback> m_errorCallbackHandle;

	TextureFormat m_depthTextureFormat = TextureFormat::Depth24Plus;
	raii::Texture m_depthTexture;
	raii::TextureView m_depthTextureView;

	raii::ShaderModule m_shaderModule;
	std::vector<RenderPipeline> m_pipelines;

	raii::Sampler m_sampler;
	raii::Texture m_texture;
	raii::TextureView m_textureView;

	tinygltf::Model m_cpuScene;
	GpuScene m_gpuScene;

	raii::Buffer m_uniformBuffer;
	raii::Buffer m_lightingUniformBuffer;
	bool m_lightingUniformsChanged = true;

	raii::BindGroupLayout m_bindGroupLayout;
	raii::BindGroupLayout m_materialBindGroupLayout;
	raii::BindGroupLayout m_nodeBindGroupLayout;

	raii::BindGroup m_bindGroup;

	ResourceManager::path m_filePath;
	bool m_filePathHasChanged;
};