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

	private:
		static VkInstance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessenger;
		static VkSurfaceKHR m_Surface;
		static LogicalDevice* m_LogicalDevice;
		VkRenderPass m_RenderPass;
		PhysicalDevice* m_PhysicalDevice;
		Swapchain* m_Swapchain;
		std::vector<Framebuffer*> m_Framebuffers;
	};
}
