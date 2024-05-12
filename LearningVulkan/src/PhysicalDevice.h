#pragma once

#include "LogicalDevice.h"

#include <vulkan/vulkan.h>

#include <optional>
#include <vector>

namespace LearningVulkan 
{
	struct QueueFamilyIndices 
	{
		std::optional<uint32_t> GraphicsFamily;
	};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR SurfaceCapabilities;
		std::vector<VkSurfaceFormatKHR> SurfaceFormats;
		std::vector<VkPresentModeKHR> PresentModes;
	};

	class PhysicalDevice
	{
	public:
		PhysicalDevice() = delete;
		~PhysicalDevice();

		static PhysicalDevice* GetSuitablePhysicalDevice();

		const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }
		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }

		static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice);
		LogicalDevice* CreateLogicalDevice();

	private:
		PhysicalDevice(VkPhysicalDevice physicalDevice);
		static QueueFamilyIndices FindQueueFamilyIndices(VkPhysicalDevice physicalDevice);
		static uint32_t RateDeviceSuitability(VkPhysicalDevice physicalDevice);
		static bool CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice);

	private:
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		QueueFamilyIndices m_QueueFamilyIndices;
	};
}
