#pragma once

#include "Window.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"

#include <vector>
#include <optional>

namespace LearningVulkan 
{
	struct QueueFamilyIndices 
	{
		// using optional because vulkan can use any value of uint32_t even 0
		std::optional<uint32_t> GraphicsFamily;
		std::optional<uint32_t> PresentationFamily;
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
		void SetupLogicalDevice();
		void SetupSurface();

	private:
		Window* m_Window;
		VkInstance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessanger;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice m_Device;

		VkQueue m_GraphicsQueue;
		VkQueue m_PresentQueue;

		VkSurfaceKHR m_Surface;
	};
}

