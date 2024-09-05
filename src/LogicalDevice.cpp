#include "LogicalDevice.h"
#include "PhysicalDevice.h"

#include <cassert>

namespace LearningVulkan 
{
	LogicalDevice::LogicalDevice(VkDevice device, PhysicalDevice* physicalDevice)
		: m_LogicalDevice(device), m_PhysicalDevice(physicalDevice)
	{
		const auto& queueFamilyIndices = m_PhysicalDevice->GetQueueFamilyIndices();

		assert(queueFamilyIndices.GraphicsFamily.has_value());
		assert(queueFamilyIndices.PresentationFamily.has_value());
		assert(queueFamilyIndices.TransferFamily.has_value());

		vkGetDeviceQueue(device, queueFamilyIndices.GraphicsFamily.value(), 0, &m_GraphicsQueue);
		vkGetDeviceQueue(device, queueFamilyIndices.PresentationFamily.value(), 0, &m_PresentQueue);
		vkGetDeviceQueue(device, queueFamilyIndices.TransferFamily.value(), 0, &m_TransferQueue);
	}

	LogicalDevice::~LogicalDevice()
	{
		vkDestroyDevice(m_LogicalDevice, nullptr);
	}

	void LogicalDevice::WaitIdle() const
	{
		vkDeviceWaitIdle(m_LogicalDevice);
	}
}
