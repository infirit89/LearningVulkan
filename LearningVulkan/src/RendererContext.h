#pragma once

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

		VkInstance GetVulkanInstance() const { return m_Instance; }

	private:
		void CreateVulkanInstance(std::string_view applicationName);
		bool CheckLayersAvailability();
		void SetupDebugMessenger();
		
	private:
		VkInstance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessenger;
	};
}
