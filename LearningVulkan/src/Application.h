#pragma once

#include "Window.h"
#include "RendererContext.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"

#include <vector>
#include <optional>

namespace LearningVulkan 
{
	struct QueueFamilyIndices 
	{
		// using optional because vulkan can use any value of uint32_t even 0
		std::optional<uint32_t> GraphicsFamily;
		std::optional<uint32_t> PresentationFamily;
	};

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
		void SetupLogicalDevice();
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
		void CreateCommandPool();
		void AllocateCommandBuffer();
		void RecordCommandBuffer(uint32_t imageIndex);
		void CreateSyncObjects();
		void DrawFrame();

	private:
		Window* m_Window;
		RendererContext* m_RenderContext;

		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice m_Device;

		VkQueue m_GraphicsQueue;
		VkQueue m_PresentQueue;

		VkSurfaceKHR m_Surface;
		VkSwapchainKHR m_Swapchain;
		std::vector<VkImage> m_SwapchainImages;
		std::vector<VkImageView> m_SwapchainImageViews;
		VkFormat m_SwapchainFormat;
		VkExtent2D m_SwapchainImagesExtent;
		VkRenderPass m_RenderPass;
		std::vector<VkFramebuffer> m_Framebuffers;
		VkCommandPool m_CommandPool;
		VkCommandBuffer m_CommandBuffer;
		VkSemaphore m_SwapchainImageAvailableSemaphore;
		VkSemaphore m_FinishedRenderingSemaphore;
		VkFence m_InFlightFence;
	};
}

