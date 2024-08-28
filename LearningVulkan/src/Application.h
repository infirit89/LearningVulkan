#pragma once

#include "Window.h"
#include "RendererContext.h"
#include "PhysicalDevice.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"

#include <vector>
#include <optional>

namespace LearningVulkan 
{
	class Application 
	{
	public:
		Application();
		~Application();

		void Run();

		static Application* Get() { return m_Instance; }
		const RendererContext* GetRenderContext() const { return m_RenderContext; }
		const Window* GetWindow() const { return m_Window; }
		float GetDeltaTime() const { return  m_DeltaTime; }

	private:
		void SetupRenderer();
		void OnResize(uint32_t width, uint32_t height);
		
	private:
		Window* m_Window;
		RendererContext* m_RenderContext;
		uint32_t m_FrameIndex = 0;
		bool m_Minimized = false;
		float m_DeltaTime = 0.0f;

		static Application* m_Instance;
	};
}
