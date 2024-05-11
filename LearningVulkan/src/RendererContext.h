#pragma once

#include "PhysicalDevice.h"

#include <vulkan/vulkan.h>

#include <string_view>
#include <vector>

namespace LearningVulkan 
{
	class RendererContext 
	{
	public:
		RendererContext(std::string_view applicationName);
		~RendererContext();

		static VkInstance GetVulkanInstance() { return m_Instance; }
		static VkSurfaceKHR GetVulkanSurface() { return m_Surface; }

		const PhysicalDevice* GetPhysicalDevice() const { return m_PhysicalDevice; }

	private:
		void CreateVulkanInstance(std::string_view applicationName);
		bool CheckLayersAvailability();
		void SetupDebugMessenger();
		void CreateSurface();

	private:
		static VkInstance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessenger;
		static VkSurfaceKHR m_Surface;
		PhysicalDevice* m_PhysicalDevice;
	};
}
