#pragma once

#include "Window.h"

#include "vulkan/vulkan.h"

#include <vector>

namespace LearningVulkan 
{
	class Application 
	{
	public:
		Application();
		~Application();

		void Run();

		std::vector<const char*> GetRequiredExtensions();
		bool CheckValidationLayerSupport();

		void SetupDebugMessanger();

	private:
		Window* m_Window;
		VkInstance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessanger;
	};
}

