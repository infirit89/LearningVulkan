#pragma once

#include "Window.h"

#include "vulkan/vulkan.h"

#include <vector>
#include <optional>

namespace LearningVulkan 
{
	struct QueueFamilyIndices 
	{
		// using optional because vulkan can use any value of uint32_t even 0
		std::optional<uint32_t> GraphicsFamily;
	};

	class Application 
	{
	public:
		Application();
		~Application();

		void Run();

	private:
		std::vector<const char*> GetRequiredExtensions();
		bool CheckValidationLayerSupport();

		void SetupDebugMessanger();
		void SetupRenderer();
		VkPhysicalDevice PickPhysicalDevice();
		uint32_t RateDeviceSuitability(VkPhysicalDevice physicalDevice);
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice);

	private:
		Window* m_Window;
		VkInstance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessanger;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
	};
}

