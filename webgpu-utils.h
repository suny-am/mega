#pragma once

#include <webgpu/webgpu.hpp>

void InspectAdapter(wgpu::Adapter adapter);

void InspectDevice(wgpu::Device device);

uint32_t CeilToNextMultiple(uint32_t value, uint32_t step);
