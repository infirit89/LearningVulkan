#pragma once

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
	}
}
