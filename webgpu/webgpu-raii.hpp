#pragma once

#include <webgpu/webgpu.hpp>

#include <type_traits>

namespace wgpu {
    namespace raii {

        // Utility template functions to detect the presence of a destroy() method
        template <typename T, typename = void>
        struct has_destroy : std::false_type { };
        template <typename T>
        struct has_destroy <T, decltype(std::declval<T>().destroy())> : std::true_type { };
        // Sanity check
        static_assert(has_destroy<wgpu::Texture>::value);
        static_assert(!has_destroy<wgpu::Queue>::value);

        /**
         * RAII wrapper around a raw WebGPU type.
         * Use pointer-like dereferencing to access methods from the wrapped type.
         */
        template <typename Raw>
        class Wrapper {
        public:
            Wrapper()
                : m_raw(nullptr)
            {}

            Wrapper(Raw&& raw)
                : m_raw(raw)
            {}

            ~Wrapper() {
                Destruct();
            }

            Wrapper& operator=(const Wrapper& other) {
                m_raw = other.m_raw;
                m_raw.reference();
                return *this;
            }

            Wrapper(const Wrapper& other) : m_raw(other.m_raw) {
                m_raw.reference();
            }

            Wrapper& operator=(Wrapper&& other) {
                Destruct();
                assert(m_raw == nullptr);
                m_raw = other.m_raw;
                other.m_raw = nullptr;
                return *this;
            }

            Wrapper(Wrapper&& other)
                : m_raw(other.m_raw)
            {
                other.m_raw = nullptr;
            }

            operator bool() const { return m_raw; }
            const Raw& operator*() const { return m_raw; }
            Raw& operator*() { return m_raw; }
            const Raw* operator->() const { return &m_raw; }
            Raw* operator->() { return &m_raw; }

        private:
            void Destruct() {
                if (!m_raw) return;

                // Call destroy() if it does exist
                if constexpr (has_destroy<Raw>::value) {
                    m_raw.destroy();
                }

                // Call release
                m_raw.release();

                m_raw = nullptr;
            }

        private:
            // Raw resources that is wrapped by the RAII class
            Raw m_raw;
        };

        using Instance = wgpu::raii::Wrapper<wgpu::Instance>;
        using Surface = wgpu::raii::Wrapper<wgpu::Surface>;
        using Adapter = wgpu::raii::Wrapper<wgpu::Adapter>;
        using Device = wgpu::raii::Wrapper<wgpu::Device>;
        using Queue = wgpu::raii::Wrapper<wgpu::Queue>;
        using RenderPassEncoder = wgpu::raii::Wrapper<wgpu::RenderPassEncoder>;
        using Texture = wgpu::raii::Wrapper<wgpu::Texture>;
        using TextureView = wgpu::raii::Wrapper<wgpu::TextureView>;
        using Buffer = wgpu::raii::Wrapper<wgpu::Buffer>;
        using CommandEncoder = wgpu::raii::Wrapper<wgpu::CommandEncoder>;
        using RenderPassEncoder = wgpu::raii::Wrapper<wgpu::RenderPassEncoder>;
        using ComputePassEncoder = wgpu::raii::Wrapper<wgpu::ComputePassEncoder>;
        using CommandBuffer = wgpu::raii::Wrapper<wgpu::CommandBuffer>;
        using BindGroup = wgpu::raii::Wrapper<wgpu::BindGroup>;
        using BindGroupLayout = wgpu::raii::Wrapper<wgpu::BindGroupLayout>;
        using RenderPipeline = wgpu::raii::Wrapper<wgpu::RenderPipeline>;
        using PipelineLayout = wgpu::raii::Wrapper<wgpu::PipelineLayout>;
        using ShaderModule = wgpu::raii::Wrapper<wgpu::ShaderModule>;
        using ComputePipeline = wgpu::raii::Wrapper<wgpu::ComputePipeline>;
        using Sampler = wgpu::raii::Wrapper<wgpu::Sampler>;
        using QuerySet = wgpu::raii::Wrapper<wgpu::QuerySet>;
        // TODO: Add other types

    } 
}