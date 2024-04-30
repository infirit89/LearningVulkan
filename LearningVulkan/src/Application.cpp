#include "Application.h"

#include "VulkanUtils.h"

#include <cassert>

#include <vector>
#include <iostream>
#include <map>
#include <set>
#include <array>
#include <algorithm>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace LearningVulkan 
{
	static constexpr size_t s_DeviceExtensionsSize = 1;
	static std::array<const char*, s_DeviceExtensionsSize> s_DeviceExtensions
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	Application::Application()
	{
		m_Window = new Window(640, 480, "i cum hard uwu");
		SetupRenderer();
	}

	Application::~Application()
	{
		vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
		for (const auto& imageView : m_SwapchainImageViews)
		{
			vkDestroyImageView(m_Device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
		vkDestroyDevice(m_Device, nullptr);
		vkDestroySurfaceKHR(m_RenderContext->GetVulkanInstance(), m_Surface, nullptr);
		delete m_RenderContext;

		delete m_Window;
	}

	void Application::Run()
	{
		while (m_Window->IsOpen())
		{
			m_Window->PollEvents();
		}
	}


	void Application::SetupRenderer()
	{
		m_RenderContext = new RendererContext("Learning Vulkan");
		SetupSurface();

		m_PhysicalDevice = PickPhysicalDevice();
		SetupLogicalDevice();
		CreateSwapchain();
		CreateImageViews();
	}

	VkPhysicalDevice Application::PickPhysicalDevice()
	{
		uint32_t physicalDeviceCount = 0;
		vkEnumeratePhysicalDevices(m_RenderContext->GetVulkanInstance(), &physicalDeviceCount, nullptr);

		assert(physicalDeviceCount > 0);

		std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
		vkEnumeratePhysicalDevices(m_RenderContext->GetVulkanInstance(), &physicalDeviceCount, physicalDevices.data());

		std::multimap<uint32_t, VkPhysicalDevice> deviceCandidates;

		for (const auto& physicalDevice : physicalDevices) 
		{
			uint32_t score = RateDeviceSuitability(physicalDevice);
			deviceCandidates.insert(std::make_pair(score, physicalDevice));
		}

		if (deviceCandidates.rbegin()->first > 0)
			return deviceCandidates.rbegin()->second;
		else 
		{
			std::cerr << "No suitable device found" << '\n';
			return nullptr;
		}
	}

	uint32_t Application::RateDeviceSuitability(VkPhysicalDevice physicalDevice)
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);


		uint32_t score = 0;

		std::cout << deviceProperties.deviceName << '\n';
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			score++;

		QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(physicalDevice);
		if (!queueFamilyIndices.GraphicsFamily.has_value())
			score = 0;

		if (!queueFamilyIndices.PresentationFamily.has_value())
			score = 0;

		if (!CheckDeviceExtensionSupport(physicalDevice))
			score = 0;
		else 
		{
			SwapChainSupportDetails details = QuerySwapChainSupport(physicalDevice);
			if (details.PresentModes.empty() || details.SurfaceFormats.empty())
				score = 0;
		}

		return score;
	}
	QueueFamilyIndices Application::FindQueueFamilies(VkPhysicalDevice physicalDevice)
	{
		QueueFamilyIndices queueFamilyIndices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

		uint32_t familyIndex = 0;
		for (const auto& queueFamily : queueFamilies) 
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				queueFamilyIndices.GraphicsFamily = familyIndex;

			VkBool32 presentationSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, m_Surface, &presentationSupport);
			if (presentationSupport)
				queueFamilyIndices.PresentationFamily = familyIndex;

			familyIndex++;
		}

		return queueFamilyIndices;
	}
	void Application::SetupLogicalDevice()
	{
		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);


		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueIndices = { indices.GraphicsFamily.value(), indices.PresentationFamily.value() };

		for (const uint32_t queueIndex : uniqueQueueIndices)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};

			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.queueFamilyIndex = queueIndex;

			float queuePriority = 1.0f;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.emplace_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};

		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.enabledExtensionCount = s_DeviceExtensionsSize;
		deviceCreateInfo.ppEnabledExtensionNames = s_DeviceExtensions.data();

		deviceCreateInfo.enabledLayerCount = VulkanUtils::LayerCount;
		deviceCreateInfo.ppEnabledLayerNames = VulkanUtils::Layers.data();

		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.queueCreateInfoCount = queueCreateInfos.size();

		assert(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device) == VK_SUCCESS);

		// the queue index is zero as we're creating only one queue from this family
		vkGetDeviceQueue(m_Device, indices.GraphicsFamily.value(), 0, &m_GraphicsQueue);

		vkGetDeviceQueue(m_Device, indices.PresentationFamily.value(), 0, &m_PresentQueue);
	}

	void Application::SetupSurface()
	{
		/*VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.hwnd = glfwGetWin32Window(m_Window->GetNativeWindow());
		surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);

		assert(vkCreateWin32SurfaceKHR(m_Instance, &surfaceCreateInfo, nullptr, &m_Surface) == VK_SUCCESS);*/

		assert(glfwCreateWindowSurface(m_RenderContext->GetVulkanInstance(), m_Window->GetNativeWindow(), nullptr, &m_Surface) == VK_SUCCESS);
	}

	bool Application::CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice)
	{
		uint32_t extensionCount = 0;
		
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> extensionProperties(extensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensionProperties.data());

		std::set<std::string> requiredExtensions(s_DeviceExtensions.begin(), s_DeviceExtensions.end());

		for (const auto& extension : extensionProperties)
			requiredExtensions.erase(extension.extensionName);

		return requiredExtensions.empty();
	}

	SwapChainSupportDetails Application::QuerySwapChainSupport(VkPhysicalDevice physicalDevice)
	{
		SwapChainSupportDetails swapChainSupport;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &swapChainSupport.SurfaceCapabilities);

		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, nullptr);

		if (formatCount > 0) 
		{
			swapChainSupport.SurfaceFormats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, swapChainSupport.SurfaceFormats.data());
		}

		uint32_t presentModesCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModesCount, nullptr);

		if (presentModesCount > 0) 
		{
			swapChainSupport.PresentModes.resize(presentModesCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModesCount, swapChainSupport.PresentModes.data());
		}

		return swapChainSupport;
	}
	VkSurfaceFormatKHR Application::ChooseCorrectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats)
	{
		for (const auto& surfaceFormat : surfaceFormats)
		{
			if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
				return surfaceFormat;
		}

		return surfaceFormats.at(0);
	}

	VkPresentModeKHR Application::ChooseSurfacePresentMode(const std::vector<VkPresentModeKHR>& presentModes)
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

	void Application::CreateSwapchain()
	{
		SwapChainSupportDetails swapchainDetails = QuerySwapChainSupport(m_PhysicalDevice);

		VkSurfaceFormatKHR surfaceFormat = ChooseCorrectSurfaceFormat(swapchainDetails.SurfaceFormats);
		VkPresentModeKHR presentMode = ChooseSurfacePresentMode(swapchainDetails.PresentModes);
		VkExtent2D framebufferExtent = ChooseSwapchainExtent(swapchainDetails.SurfaceCapabilities);

		uint32_t minImageCount = swapchainDetails.SurfaceCapabilities.minImageCount + 1;
		if (swapchainDetails.SurfaceCapabilities.maxImageCount > 0 && minImageCount > swapchainDetails.SurfaceCapabilities.maxImageCount)
			minImageCount = swapchainDetails.SurfaceCapabilities.maxImageCount;

		VkSwapchainCreateInfoKHR swapchainCreateInfo{};
		swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCreateInfo.imageExtent = framebufferExtent;
		swapchainCreateInfo.presentMode = presentMode;
		swapchainCreateInfo.imageFormat = surfaceFormat.format;
		swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
		swapchainCreateInfo.surface = m_Surface;
		swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchainCreateInfo.imageArrayLayers = 1;
		swapchainCreateInfo.minImageCount = minImageCount;

		QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

		uint32_t indices[] = { queueFamilyIndices.GraphicsFamily.value(), queueFamilyIndices.PresentationFamily.value() };

		if (queueFamilyIndices.GraphicsFamily != queueFamilyIndices.PresentationFamily) 
		{
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchainCreateInfo.queueFamilyIndexCount = 2;
			swapchainCreateInfo.pQueueFamilyIndices = indices;
		}
		else 
		{
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchainCreateInfo.queueFamilyIndexCount = 0;
			swapchainCreateInfo.pQueueFamilyIndices = nullptr;
		}

		swapchainCreateInfo.preTransform = swapchainDetails.SurfaceCapabilities.currentTransform;
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.clipped = VK_TRUE;
		swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

		assert(vkCreateSwapchainKHR(m_Device, &swapchainCreateInfo, nullptr, &m_Swapchain) == VK_SUCCESS);

		uint32_t imageCount;
		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
		m_SwapchainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());

		m_SwapchainFormat = surfaceFormat.format;
		m_SwapchainImagesExtent = framebufferExtent;
	}

	void Application::CreateImageViews()
	{
		m_SwapchainImageViews.resize(m_SwapchainImages.size());

		for (size_t i = 0; i < m_SwapchainImages.size(); i++)
		{
			VkImageViewCreateInfo imageViewCreateInfo{};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.image = m_SwapchainImages.at(i);
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = m_SwapchainFormat;
			imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;
			assert(vkCreateImageView(m_Device, &imageViewCreateInfo, nullptr, &m_SwapchainImageViews.at(i)) == VK_SUCCESS);
		}
	}

	void Application::CreateRenderPass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = m_SwapchainFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		VkRenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = 1;
		renderPassCreateInfo.pAttachments = &colorAttachment;
		renderPassCreateInfo.subpassCount = 0;

		assert(vkCreateRenderPass(m_Device, &renderPassCreateInfo, nullptr, &m_RenderPass) == VK_SUCCESS);
	}

	VkExtent2D Application::ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
	{
		if (surfaceCapabilities.currentExtent.width != UINT32_MAX)
			return surfaceCapabilities.currentExtent;

		uint32_t width, height;
		glfwGetFramebufferSize(m_Window->GetNativeWindow(), (int*) & width, (int*) & height);

		VkExtent2D extent = {
			width,
			height
		};

		extent.width = std::clamp(extent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
		extent.height = std::clamp(extent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);

		return extent;
	}
}
