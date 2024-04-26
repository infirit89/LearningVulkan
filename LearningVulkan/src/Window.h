#pragma once

#include <cstdint>

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

	private:
		GLFWwindow* m_Window;
	};
}
