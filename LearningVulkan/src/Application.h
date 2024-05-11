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
	//struct QueueFamilyIndices 
	//{
	//	// using optional because vulkan can use any value of uint32_t even 0
	//	std::optional<uint32_t> GraphicsFamily;
	//	std::optional<uint32_t> PresentationFamily;
	//};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR SurfaceCapabilities;
		std::vector<VkSurfaceFormatKHR> SurfaceFormats;
		std::vector<VkPresentModeKHR> PresentModes;
	};

	class Application 
	{
	public:
		Application();
		~Application();

		void Run();

	private:
		void SetupRenderer();
		VkPhysicalDevice PickPhysicalDevice();
		uint32_t RateDeviceSuitability(VkPhysicalDevice physicalDevice);
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice);
		//void SetupLogicalDevice();
		void SetupSurface();
		bool CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice);
		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice);
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
		PhysicalDevice* m_PhysicalDevice;

		//VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		//VkDevice m_Device;

		/*VkQueue m_GraphicsQueue;
		VkQueue m_PresentQueue;*/

		VkSurfaceKHR m_Surface;
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
	};
}

