#include "Window.h"

#include <assert.h>

#include <optick.h>

namespace LearningVulkan 
{
    Window::Window(uint32_t width, uint32_t height, const char* title)
    {
        assert(glfwInit());

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);

        assert(m_Window);
    }

    Window::~Window()
    {
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }

    void Window::PollEvents()
    {
        OPTICK_EVENT();
        glfwPollEvents();
    }

    bool Window::IsOpen()
    {
        return !glfwWindowShouldClose(m_Window);
    }
}

