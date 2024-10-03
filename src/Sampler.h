#pragma once
#include <vulkan/vulkan_core.h>

namespace LearningVulkan
{

    enum class TextureFilter
    {
        Nearest = 0,
        Linear = 1,
    };

    enum class TextureAddressMode
    {
        Repeat = 0,
        MirroredRepeat = 1,
        ClampToEdge = 2,
        ClampToBorder = 3,
        MirroredClampToEdge = 4,
    };

    struct SamplerCreateInfo
    {
        TextureFilter MagFilter;
        TextureFilter MinFilter;
        TextureFilter MipmapFilter;

        TextureAddressMode AddressModeU;
        TextureAddressMode AddressModeV;
        TextureAddressMode AddressModeW;
        bool AnisotropyEnable;
    };

    class Sampler
    {
    public:
        Sampler(const SamplerCreateInfo& createInfo);
        ~Sampler();

        Sampler(const Sampler& other) = delete;
        Sampler(Sampler&& other) = delete;
        Sampler& operator=(const Sampler& other) = delete;

        const VkSampler& GetVulkanSampler() const { return m_Sampler; }

    private:
        void Create(const SamplerCreateInfo& createInfo);

    private:
        VkSampler m_Sampler;
    };
}
