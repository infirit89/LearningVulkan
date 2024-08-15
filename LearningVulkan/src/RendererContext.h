#pragma once

#include "PhysicalDevice.h"
#include "Swapchain.h"
#include "Framebuffer.h"

#include <vulkan/vulkan.h>

#include <string_view>
#include <vector>

namespace LearningVulkan 
{
	// TODO: move inside render context
	struct PerFrameData
	{
		VkCommandBuffer CommandBuffer;
		VkCommandPool CommandPool;
		VkFence PresentFence;
		VkSemaphore SwapchainImageAcquireSemaphore;
		VkSemaphore QueueReadySemaphore;
	};

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

		void Resize(uint32_t width, uint32_t height) const;
		const std::vector<Framebuffer*>& GetFramebuffers() const { return m_Framebuffers; }
		const PerFrameData& GetPerFrameData(size_t index) const { return m_PerFrameData.at(index); }
		size_t GetPerFrameDataSize() const { return m_PerFrameData.size(); }
		const VkPipeline& GetGraphicsPipeline() const { return m_Pipeline; }

	private:
		static void CreateVulkanInstance(std::string_view applicationName);
		static bool CheckLayersAvailability();
		void SetupDebugMessenger();
		static void CreateSurface();
		void CreateRenderPass();

		VkCommandPool CreateCommandPool() const;

		static VkCommandBuffer AllocateCommandBuffer(VkCommandPool commandPool);
		static void RecordCommandBuffer(uint32_t imageIndex, VkCommandBuffer commandBuffer);
		static void CreateSyncObjects(VkSemaphore& swapchainImageAcquireSemaphore, VkSemaphore& queueReadySemaphore, VkFence& presentFence);

		void CreatePerFrameObjects(uint32_t frameIndex);
		void DrawFrame();
		void CreateGraphicsPipeline();
		static VkShaderModule CreateShader(const std::vector<char>& shaderData);

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

		std::vector<PerFrameData> m_PerFrameData;
	};
}
