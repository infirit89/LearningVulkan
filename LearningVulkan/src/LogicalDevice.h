#pragma once

#include <vulkan/vulkan.h>

namespace LearningVulkan 
{
	class LogicalDevice 
	{
	public:
		~LogicalDevice();
		VkDevice GetVulkanDevice() const { return m_LogicalDevice; }

		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		void WaitIdle();

	private:
		LogicalDevice(VkDevice device);

	private:
		VkDevice m_LogicalDevice = VK_NULL_HANDLE;
		VkQueue m_GraphicsQueue = VK_NULL_HANDLE;

		friend class PhysicalDevice;
	};
}
