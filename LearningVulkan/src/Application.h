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
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice);
		//void SetupLogicalDevice();
		VkSurfaceFormatKHR ChooseCorrectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats);
		VkPresentModeKHR ChooseSurfacePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
		VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
		void CreateSwapchain();
		void CreateImageViews();
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
		void DestroySwapchain(VkSwapchainKHR swapchain);
		void OnResize(uint32_t width, uint32_t height);

	private:
		Window* m_Window;
		RendererContext* m_RenderContext;
		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		std::vector<VkImage> m_SwapchainImages;
		std::vector<VkImageView> m_SwapchainImageViews;
		VkSurfaceFormatKHR m_SurfaceFormat;
		VkExtent2D m_SwapchainImagesExtent;
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
		SwapChainSupportDetails m_SwapchainDetails;
		bool m_Minimized = false;

		static Application* m_Instance;
	};
}

