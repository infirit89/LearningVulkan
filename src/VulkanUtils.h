#pragma once

#include <vulkan/vulkan.h>
#include <array>

namespace LearningVulkan 
{
    namespace VulkanUtils 
    {
        inline constexpr size_t LayerCount = 1;
        inline std::array<const char*, LayerCount> Layers
        {
            "VK_LAYER_KHRONOS_validation",
        };

        inline constexpr size_t DeviceExtensionsSize = 1;
        inline std::array<const char*, DeviceExtensionsSize> DeviceExtensions
        {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
    }
}
