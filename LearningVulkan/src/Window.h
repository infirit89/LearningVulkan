#pragma once

#include <cstdint>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace LearningVulkan 
{
	class Window 
	{
	public:
		Window(uint32_t width, uint32_t height, const char* title);
		~Window();

		void PollEvents();
		bool IsOpen();

		GLFWwindow* GetNativeWindow() const { return m_Window; }

	private:
		GLFWwindow* m_Window;
	};
}
