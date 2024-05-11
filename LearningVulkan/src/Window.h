#pragma once

#include <cstdint>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <functional>

namespace LearningVulkan 
{
	class Window 
	{
	public:
		using ResizeFn = std::function<void(uint32_t, uint32_t)>;
		Window(uint32_t width, uint32_t height, const char* title);
		~Window();

		void PollEvents();
		bool IsOpen();

		GLFWwindow* GetNativeWindow() const { return m_Window; }

		void SetResizeFn(const ResizeFn& resizeFn) { m_ResizeFn = resizeFn; }

	private:
		ResizeFn m_ResizeFn;
		GLFWwindow* m_Window;
	};
}
