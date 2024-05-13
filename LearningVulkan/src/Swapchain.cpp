#include "Swapchain.h"
#include "RendererContext.h"
#include "LogicalDevice.h"

#include <iostream>
#include <algorithm>
#include <cassert>

namespace LearningVulkan 
{
	static VkPresentModeKHR ConvertToVkPresentMode(PresentMode presentMode) 
	{
		switch (presentMode)
		{
		case LearningVulkan::PresentMode::Mailbox: return VK_PRESENT_MODE_MAILBOX_KHR;
		case LearningVulkan::PresentMode::Fifo: return VK_PRESENT_MODE_FIFO_KHR;
		case LearningVulkan::PresentMode::Imediate: return VK_PRESENT_MODE_IMMEDIATE_KHR;
		}

		std::cerr << "Couldn't convert to vk present mode defaulting to fifo\n";
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	Swapchain::Swapchain(LogicalDevice* logicalDevice, uint32_t width, uint32_t height, PresentMode presentMode)
		: m_Width(width), m_Height(height), m_DesiredPresentMode(presentMode), m_LogicalDevice(logicalDevice)
	{
		Create();
	}

	Swapchain::~Swapchain()
	{
		Destroy(m_Swapchain);
	}

	void Swapchain::Recreate(uint32_t width, uint32_t height)
	{
		m_Width = width;
		m_Height = height;
		Create();
	}

	void Swapchain::Present(VkSemaphore semaphore, uint32_t imageIndex)
	{
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &semaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_Swapchain;
		presentInfo.pImageIndices = &imageIndex;

		vkQueuePresentKHR(m_LogicalDevice->GetPresentQueue(), &presentInfo);
	}

	void Swapchain::AcquireNextImage(VkSemaphore imageAcquireSemaphore, uint32_t& imageIndex)
	{
		vkAcquireNextImageKHR(m_LogicalDevice->GetVulkanDevice(), m_Swapchain, UINT64_MAX, 
							imageAcquireSemaphore, VK_NULL_HANDLE, &imageIndex);
	}

	void Swapchain::Create()
	{
		const SwapchainSupportDetails& swapchainDetails = m_LogicalDevice->GetPhysicalDevice()->QuerySwapChainSupport();
		VkPresentModeKHR presentMode = ChooseSurfacePresentMode(swapchainDetails.PresentModes);
		m_Extent = ChooseSwapchainExtent(swapchainDetails.SurfaceCapabilities);
		m_SurfaceFormat = ChooseCorrectSurfaceFormat(swapchainDetails.SurfaceFormats);

		const VkSurfaceCapabilitiesKHR& surfaceCapabilities = swapchainDetails.SurfaceCapabilities;
		uint32_t minImageCount = surfaceCapabilities.minImageCount + 1;
		if (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount)
			minImageCount = surfaceCapabilities.maxImageCount;

		VkSwapchainKHR oldSwapchain = m_Swapchain;

		VkSwapchainCreateInfoKHR swapchainCreateInfo{};
		swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCreateInfo.imageExtent = m_Extent;
		swapchainCreateInfo.surface = RendererContext::GetVulkanSurface();
		swapchainCreateInfo.presentMode = ConvertToVkPresentMode(m_DesiredPresentMode);
		swapchainCreateInfo.imageFormat = m_SurfaceFormat.format;
		swapchainCreateInfo.imageColorSpace = m_SurfaceFormat.colorSpace;
		swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchainCreateInfo.imageArrayLayers = 1;
		swapchainCreateInfo.minImageCount = minImageCount;

		const QueueFamilyIndices& queueFamilyIndices = m_LogicalDevice->GetPhysicalDevice()->GetQueueFamilyIndices();

		uint32_t indices[] = { queueFamilyIndices.GraphicsFamily.value(), queueFamilyIndices.PresentationFamily.value() };

		if (queueFamilyIndices.GraphicsFamily != queueFamilyIndices.PresentationFamily)
		{
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchainCreateInfo.queueFamilyIndexCount = 2;
			swapchainCreateInfo.pQueueFamilyIndices = indices;
		}
		else
		{
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swapchainCreateInfo.queueFamilyIndexCount = 0;
			swapchainCreateInfo.pQueueFamilyIndices = nullptr;
		}

		swapchainCreateInfo.preTransform = swapchainDetails.SurfaceCapabilities.currentTransform;
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.clipped = VK_TRUE;
		swapchainCreateInfo.oldSwapchain = oldSwapchain;

		assert(vkCreateSwapchainKHR(m_LogicalDevice->GetVulkanDevice(), &swapchainCreateInfo, nullptr, &m_Swapchain) == VK_SUCCESS);

		if (oldSwapchain != VK_NULL_HANDLE)
			Destroy(oldSwapchain);

		uint32_t imageCount;
		vkGetSwapchainImagesKHR(m_LogicalDevice->GetVulkanDevice(), m_Swapchain, &imageCount, nullptr);
		m_Images.resize(imageCount);
		vkGetSwapchainImagesKHR(m_LogicalDevice->GetVulkanDevice(), m_Swapchain, &imageCount, m_Images.data());

		m_ImageViews.resize(imageCount);

		for (size_t i = 0; i < m_Images.size(); i++)
		{
			VkImageViewCreateInfo imageViewCreateInfo{};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.image = m_Images.at(i);
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = m_SurfaceFormat.format;
			imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;
			assert(vkCreateImageView(m_LogicalDevice->GetVulkanDevice(), &imageViewCreateInfo, nullptr, &m_ImageViews.at(i)) == VK_SUCCESS);
		}
	}

	void Swapchain::Destroy(VkSwapchainKHR swapchain)
	{
		for (const auto& imageViews : m_ImageViews)
			vkDestroyImageView(m_LogicalDevice->GetVulkanDevice(), imageViews, nullptr);

		vkDestroySwapchainKHR(m_LogicalDevice->GetVulkanDevice(), swapchain, nullptr);
	}

	VkSurfaceFormatKHR Swapchain::ChooseCorrectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats) 
	{
		for (const auto& surfaceFormat : surfaceFormats)
		{
			if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
				return surfaceFormat;
		}

		return surfaceFormats.at(0);
	}

	VkPresentModeKHR Swapchain::ChooseSurfacePresentMode(const std::vector<VkPresentModeKHR>& presentModes) 
	{
		for (const auto& presentMode : presentModes)
		{
			// present images last in first out
			if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				return presentMode;
		}

		// present images first in first out
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D Swapchain::ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) 
	{
		VkExtent2D extent = {
			m_Width,
			m_Height
		};

		extent.width = std::clamp(extent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
		extent.height = std::clamp(extent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);

		return extent;
	}
}
