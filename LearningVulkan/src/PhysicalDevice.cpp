#include "PhysicalDevice.h"
#include "VulkanUtils.h"

#include <vector>
#include <cassert>

namespace LearningVulkan
{
	PhysicalDevice::PhysicalDevice(VkPhysicalDevice physicalDevice)
	{
		m_PhysicalDevice = physicalDevice;
		SetupLogicalDevice();
	}

	void PhysicalDevice::SetupLogicalDevice()
	{
		m_QueueFamilyIndices = FindQueueFamilyIndices();

		VkDeviceQueueCreateInfo graphicsQueueCreateInfo{};
		graphicsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		graphicsQueueCreateInfo.queueFamilyIndex = m_QueueFamilyIndices.GraphicsFamily.value();
		graphicsQueueCreateInfo.queueCount = 1;
		float queuePriority = 1.0f;
		graphicsQueueCreateInfo.pQueuePriorities = &queuePriority;

		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		deviceCreateInfo.enabledLayerCount = VulkanUtils::LayerCount;
		deviceCreateInfo.ppEnabledLayerNames = VulkanUtils::Layers.data();

		deviceCreateInfo.queueCreateInfoCount = 1;
		deviceCreateInfo.pQueueCreateInfos = &graphicsQueueCreateInfo;

		deviceCreateInfo.enabledExtensionCount = VulkanUtils::DeviceExtensionsSize;
		deviceCreateInfo.ppEnabledExtensionNames = VulkanUtils::DeviceExtensions.data();

		assert(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device) == VK_SUCCESS);

		vkGetDeviceQueue(m_Device, m_QueueFamilyIndices.GraphicsFamily.value(), 0, &m_GraphicsQueue);
	}

	QueueFamilyIndices PhysicalDevice::FindQueueFamilyIndices()
	{
		QueueFamilyIndices queueFamilyIndices;
		uint32_t queueFamilyPropertiesCount;
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyPropertiesCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertiesCount);
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyPropertiesCount, queueFamilyProperties.data());

		uint32_t index = 0;
		for (const auto& properties : queueFamilyProperties) 
		{
			if (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				queueFamilyIndices.GraphicsFamily = index;

			//vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, index, )
			index++;
		}

		return queueFamilyIndices;
	}

	PhysicalDevice::~PhysicalDevice()
	{
		vkDestroyDevice(m_Device, nullptr);
	}
}

