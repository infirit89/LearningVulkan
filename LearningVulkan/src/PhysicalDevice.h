#pragma once

#include <vulkan/vulkan.h>

#include <optional>

namespace LearningVulkan 
{
	struct QueueFamilyIndices 
	{
		std::optional<uint32_t> GraphicsFamily, PresentFamily;
	};

	class PhysicalDevice
	{
	public:
		PhysicalDevice(VkPhysicalDevice physicalDevice);
		~PhysicalDevice();
		VkDevice GetLogicalDevice() const { return m_Device; }
		const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }
		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }

	private:
		void SetupLogicalDevice();
		QueueFamilyIndices FindQueueFamilyIndices();

	private:
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice m_Device = VK_NULL_HANDLE;
		QueueFamilyIndices m_QueueFamilyIndices;
		VkQueue m_GraphicsQueue;

		friend class RenderContext;
	};
}
