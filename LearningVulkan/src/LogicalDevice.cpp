#include "LogicalDevice.h"
#include "PhysicalDevice.h"

namespace LearningVulkan 
{
	LogicalDevice::LogicalDevice(VkDevice device, PhysicalDevice* physicalDevice)
		: m_LogicalDevice(device), m_PhysicalDevice(physicalDevice)
	{
		const auto& queueFamilyIndices = m_PhysicalDevice->GetQueueFamilyIndices();
		vkGetDeviceQueue(device, queueFamilyIndices.GraphicsFamily.value(), 0, &m_GraphicsQueue);
		vkGetDeviceQueue(device, queueFamilyIndices.PresentationFamily.value(), 0, &m_PresentQueue);
	}

	LogicalDevice::~LogicalDevice()
	{
		vkDestroyDevice(m_LogicalDevice, nullptr);
	}

	void LogicalDevice::WaitIdle()
	{
		vkDeviceWaitIdle(m_LogicalDevice);
	}
}
