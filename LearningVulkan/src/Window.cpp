#include "Window.h"

#include <assert.h>

#include <optick.h>

namespace LearningVulkan 
{
    Window::Window(uint32_t width, uint32_t height, const char* title)
        : m_Width(width), m_Height(height)
    {
        assert(glfwInit());

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);

        assert(m_Window);
        glfwSetWindowUserPointer(m_Window, &m_ResizeFn);

        glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
        {
            ResizeFn callback = *static_cast<ResizeFn*>(glfwGetWindowUserPointer(window));
            callback(width, height);
        });
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

