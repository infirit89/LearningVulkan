#pragma once

#include <vulkan/vulkan.h>

#include <optional>
#include <vector>

namespace LearningVulkan 
{
	struct QueueFamilyIndices 
	{
		std::optional<uint32_t> GraphicsFamily, PresentFamily;
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

		VkDevice GetLogicalDevice() const { return m_Device; }
		const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }
		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }

		static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice);
	private:
		PhysicalDevice(VkPhysicalDevice physicalDevice);
		void SetupLogicalDevice();
		static QueueFamilyIndices FindQueueFamilyIndices(VkPhysicalDevice physicalDevice);
		static uint32_t RateDeviceSuitability(VkPhysicalDevice physicalDevice);
		static bool CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice);

	private:
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice m_Device = VK_NULL_HANDLE;
		QueueFamilyIndices m_QueueFamilyIndices;
		VkQueue m_GraphicsQueue;
	};
}
