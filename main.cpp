// utilities
#include "webgpu-utils.h"

#include <webgpu/webgpu.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <iostream>
#include <cassert>
#include <vector>

int main(int, char **)
{
	WGPUInstanceDescriptor desc = {};
	desc.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_DAWN
	// Make sure the uncaputed error callback is called as soon as an error
	// occurs, rather than at the next call to "wgpuDeviceTick"
	WGPUDawnTogglesDescriptor toggles;
	toggles.chain.next = nullptr;
	toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
	toggles.disabledTogglesCount = 0;
	toggles.enabledTogglesCount = 1;
	const char *toggleName = "enable_immediate_error_handling";
	toggles.enabledToggles = &toggleName;

	instanceDesc.nextInChain = &toggles.chain;
#endif // WEBGPU_BACKEND_DAWN

#ifdef WEBGPU_BACKEND_EMSCRIPTEN
	WGPUInstance instance = wgpuCreateInstance(nullptr);
#else  // WEBGPU_BACKEND_EMSCRIPTEN
	WGPUInstance instance = wgpuCreateInstance(&desc);
#endif // WEBGPU_BACKEND_EMSCRIPTEN

	if (!instance)
	{
		std::cerr << "Could not initialize WebGPU." << std::endl;
	}

	std::cout << "WGPU instance: " << instance << std::endl;

	std::cout << "Requesting adapter..." << std::endl;
	WGPURequestAdapterOptions adapterOpts = {};
	adapterOpts.nextInChain = nullptr;
	WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);
	std::cout << "Got adapter: " << adapter << std::endl;

	inspectAdapter(adapter);

	// Instance is no no longer needed after acquiring the adapter
	wgpuInstanceRelease(instance);

	std::cout << "Requesting device..." << std::endl;
	WGPUDeviceDescriptor deviceDesc = {};
	deviceDesc.nextInChain = nullptr;
	deviceDesc.label = "iGPU Device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = nullptr;
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "default queue";
	// default callback used when device is no longer available for whatever reason
	deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const *message, void * /* pUserData */)
	{
		std::cout << "Device lost: reason " << reason;
		if (message)
			std::cout << " (" << message << ")";
		std::cout << std::endl;
	};

	WGPUDevice device = requestDeviceSync(adapter, &deviceDesc);
	std::cout << "Got device: " << device << std::endl;
	
	// default callback for whenener an error occurs when using the device
	auto onDeviceError = [](WGPUErrorType type, char const *message, void * /* pUserData*/)
	{
		std::cout << "Uncaptured device error: type " << type;
		if (message)
			std::cout << " (" << message << ")";
		std::cout << std::endl;
	};
	wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr /* pUserData */);

	wgpuAdapterRelease(adapter);

	inspectDevice(device);

	wgpuDeviceRelease(device);

	return 0;
}
