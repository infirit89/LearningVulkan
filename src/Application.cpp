#include "Application.h"

#include <cassert>

#include <vector>
#include <iostream>
#include <map>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <optick.h>

namespace LearningVulkan 
{
    Application* Application::m_Instance;

    Application::Application()
    {
        m_Instance = this;
        m_Window = new Window(640, 480, "i cum hard uwu");

        m_Window->SetResizeFn(std::bind_front(&Application::OnResize, this));
        glfwSetInputMode(m_Window->GetNativeWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        SetupRenderer();
    }

    Application::~Application()
    {
        delete m_RenderContext;

        delete m_Window;
    }

    void Application::Run()
    {
        float currentFrameTime = 0.0f, lastFrameTime = 0.0f;
        while (m_Window->IsOpen())
        {
            OPTICK_FRAME("MainThread");
            currentFrameTime = glfwGetTime();
            m_DeltaTime = currentFrameTime - lastFrameTime;
            lastFrameTime = currentFrameTime;

            if (!m_Minimized)
                m_RenderContext->DrawFrame();

            m_Window->PollEvents();
        }

        m_RenderContext->GetLogicalDevice()->WaitIdle();
    }

    void Application::SetupRenderer()
    {
        m_RenderContext = new RendererContext("Learning Vulkan");
    }

    void Application::OnResize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0) 
        {
            m_Minimized = true;
            return;
        }

        m_Minimized = false;
        m_RenderContext->Resize(width, height);
    }
}
