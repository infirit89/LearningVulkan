#pragma once

#include "PhysicalDevice.h"
#include "Swapchain.h"
#include "Framebuffer.h"

#include <vulkan/vulkan.h>

#include <string_view>
#include <vector>

namespace LearningVulkan 
{
	class RendererContext 
	{
	public:
		RendererContext(std::string_view applicationName);
		~RendererContext();

		static VkInstance GetVulkanInstance() { return m_Instance; }
		static VkSurfaceKHR GetVulkanSurface() { return m_Surface; }

		const PhysicalDevice* GetPhysicalDevice() const { return m_PhysicalDevice; }
		static LogicalDevice* GetLogicalDevice() { return m_LogicalDevice; }
		Swapchain* GetSwapchain() const { return m_Swapchain; }
		VkRenderPass GetRenderPass() const { return m_RenderPass; }

		void Resize(uint32_t width, uint32_t height);
		const std::vector<Framebuffer*>& GetFramebuffers() const { return m_Framebuffers; }

	private:
		void CreateVulkanInstance(std::string_view applicationName);
		bool CheckLayersAvailability();
		void SetupDebugMessenger();
		void CreateSurface();
		void CreateRenderPass();

		VkCommandPool CreateCommandPool();

		VkCommandBuffer AllocateCommandBuffer(VkCommandPool commandPool);
		void RecordCommandBuffer(uint32_t imageIndex, VkCommandBuffer commandBuffer);
		void CreateSyncObjects(VkSemaphore& swapchainImageAcquireSemaphore, VkSemaphore& queueReadySemaphore, VkFence& presentFence);

		void CreatePerFrameObjects(uint32_t frameIndex);
		void DrawFrame();
		void CreateGraphicsPipeline();
		VkShaderModule CreateShader(const std::vector<char>& shaderData);

	private:
		static VkInstance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessenger;
		static VkSurfaceKHR m_Surface;
		static LogicalDevice* m_LogicalDevice;
		VkRenderPass m_RenderPass;
		PhysicalDevice* m_PhysicalDevice;
		Swapchain* m_Swapchain;
		std::vector<Framebuffer*> m_Framebuffers;

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
	};
}
