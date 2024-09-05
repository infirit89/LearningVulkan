#pragma once

#include <vulkan/vulkan.h>

namespace LearningVulkan 
{
	class PhysicalDevice;
	class LogicalDevice 
	{
	public:
		~LogicalDevice();
		VkDevice GetVulkanDevice() const { return m_LogicalDevice; }

		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		VkQueue GetPresentQueue() const { return m_PresentQueue; }
		VkQueue GetTransferQueue() const { return m_TransferQueue; }
		void WaitIdle() const;
		PhysicalDevice* GetPhysicalDevice() const { return m_PhysicalDevice; }

	private:
		LogicalDevice(VkDevice device, PhysicalDevice* physicalDevice);

	private:
		VkDevice m_LogicalDevice = VK_NULL_HANDLE;
		VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
		VkQueue m_PresentQueue = VK_NULL_HANDLE;
		VkQueue m_TransferQueue = VK_NULL_HANDLE;
		PhysicalDevice* m_PhysicalDevice = nullptr;

		friend class PhysicalDevice;
	};
}
