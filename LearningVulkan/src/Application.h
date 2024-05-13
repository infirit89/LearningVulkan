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

	private:
		void SetupRenderer();
		void CreateRenderPass();
		void CreateFramebuffers();
		VkCommandPool CreateCommandPool();

		VkCommandBuffer AllocateCommandBuffer(VkCommandPool commandPool);
		void RecordCommandBuffer(uint32_t imageIndex, VkCommandBuffer commandBuffer);
		void CreateSyncObjects(VkSemaphore& swapchainImageAcquireSemaphore, VkSemaphore& queueReadySemaphore, VkFence& presentFence);

		void CreatePerFrameObjects(uint32_t frameIndex);
		void DrawFrame();
		void CreateGraphicsPipeline();
		VkShaderModule CreateShader(const std::vector<char>& shaderData);
		void OnResize(uint32_t width, uint32_t height);
		void DestroyFramebuffers();

	private:
		Window* m_Window;
		RendererContext* m_RenderContext;
		VkRenderPass m_RenderPass;
		std::vector<VkFramebuffer> m_Framebuffers;
		VkPipelineLayout m_PipelineLayout;
		VkPipeline m_Pipeline;

		struct PerFrameData
		{
			VkCommandBuffer CommandBuffer;
			VkCommandPool CommandPool;
			VkFence PresentFence;
			VkSemaphore SwapchainImageAcquireSemaphore;
			VkSemaphore QueueReadySemaphore;
		};

		std::vector<PerFrameData> m_PerFrameData;
		uint32_t m_FrameIndex = 0;
		bool m_Minimized = false;

		static Application* m_Instance;
	};
}
