#include "LogicalDevice.h"

namespace LearningVulkan 
{
	LogicalDevice::LogicalDevice(VkDevice device)
		: m_LogicalDevice(device)
	{
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
