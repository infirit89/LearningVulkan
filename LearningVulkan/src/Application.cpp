#include "Application.h"

#include "VulkanUtils.h"

#include <cassert>

#include <vector>
#include <iostream>
#include <map>
#include <set>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <optick.h>

namespace LearningVulkan 
{
	static constexpr size_t s_DeviceExtensionsSize = 1;
	static std::array<const char*, s_DeviceExtensionsSize> s_DeviceExtensions
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	static std::vector<char> ReadShaderFile(const std::filesystem::path& shaderPath) 
	{
		std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);

		assert(file.is_open());

		size_t fileSize = file.tellg();
		std::vector<char> fileData(fileSize);
		file.seekg(0);
		file.read(fileData.data(), fileSize);

		return fileData;
	}

	Application::Application()
	{
		m_Window = new Window(640, 480, "i cum hard uwu");

		m_Window->SetResizeFn(std::bind(&Application::OnResize, this, std::placeholders::_1, std::placeholders::_2));
		SetupRenderer();
	}

	Application::~Application()
	{
		for (const auto& data : m_PerFrameData)
		{
			vkDestroySemaphore(
				m_Device,
				data.SwapchainImageAcquireSemaphore,
				nullptr);
			vkDestroySemaphore(
				m_Device,
				data.QueueReadySemaphore,
				nullptr);
			vkDestroyFence(m_Device, data.PresentFence, nullptr);
			vkFreeCommandBuffers(
				m_Device, data.CommandPool, 1, &data.CommandBuffer);
			vkDestroyCommandPool(m_Device, data.CommandPool, nullptr);
		}

		vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
		vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);

		for (const auto& framebuffer : m_Framebuffers)
			vkDestroyFramebuffer(m_Device, framebuffer, nullptr);

		vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

		for (const auto& imageView : m_SwapchainImageViews)
			vkDestroyImageView(m_Device, imageView, nullptr);

		vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
		vkDestroyDevice(m_Device, nullptr);
		vkDestroySurfaceKHR(m_RenderContext->GetVulkanInstance(), m_Surface, nullptr);
		delete m_RenderContext;

		delete m_Window;
	}

	void Application::Run()
	{
		float currentFrameTime = 0.0f, lastFrameTime = 0.0f;
		while (m_Window->IsOpen())
		{
			OPTICK_FRAME("MainThread");
			currentFrameTime = glfwGetTime();
			float deltaTime = currentFrameTime - lastFrameTime;
			lastFrameTime = currentFrameTime;

			if(!m_Minimized)
				DrawFrame();

			m_Window->PollEvents();
		}

		vkDeviceWaitIdle(m_Device);
	}


	void Application::SetupRenderer()
	{
		m_RenderContext = new RendererContext("Learning Vulkan");
		SetupSurface();

		m_PhysicalDevice = PickPhysicalDevice();
		SetupLogicalDevice();
		m_SwapchainDetails = QuerySwapChainSupport(m_PhysicalDevice);
		m_SurfaceFormat = ChooseCorrectSurfaceFormat(m_SwapchainDetails.SurfaceFormats);
		CreateRenderPass();
		CreateSwapchain();
		m_PerFrameData.resize(m_SwapchainImages.size());
		for (uint32_t i = 0; i < m_SwapchainImages.size(); i++)
			CreatePerFrameObjects(i);

		CreateGraphicsPipeline();
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
		m_SwapchainDetails = QuerySwapChainSupport(m_PhysicalDevice);
		VkPresentModeKHR presentMode = ChooseSurfacePresentMode(m_SwapchainDetails.PresentModes);
		VkExtent2D framebufferExtent = ChooseSwapchainExtent(m_SwapchainDetails.SurfaceCapabilities);

		uint32_t minImageCount = m_SwapchainDetails.SurfaceCapabilities.minImageCount + 1;
		if (m_SwapchainDetails.SurfaceCapabilities.maxImageCount > 0 && minImageCount > m_SwapchainDetails.SurfaceCapabilities.maxImageCount)
			minImageCount = m_SwapchainDetails.SurfaceCapabilities.maxImageCount;

		VkSwapchainKHR oldSwapchain = m_Swapchain;

		VkSwapchainCreateInfoKHR swapchainCreateInfo{};
		swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCreateInfo.imageExtent = framebufferExtent;
		swapchainCreateInfo.presentMode = presentMode;
		swapchainCreateInfo.imageFormat = m_SurfaceFormat.format;
		swapchainCreateInfo.imageColorSpace = m_SurfaceFormat.colorSpace;
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
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swapchainCreateInfo.queueFamilyIndexCount = 1;
			swapchainCreateInfo.pQueueFamilyIndices = &queueFamilyIndices.GraphicsFamily.value();
		}

		swapchainCreateInfo.preTransform = m_SwapchainDetails.SurfaceCapabilities.currentTransform;
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.clipped = VK_TRUE;
		swapchainCreateInfo.oldSwapchain = oldSwapchain;

		assert(vkCreateSwapchainKHR(m_Device, &swapchainCreateInfo, nullptr, &m_Swapchain) == VK_SUCCESS);

		if (oldSwapchain != VK_NULL_HANDLE)
			DestroySwapchain(oldSwapchain);

		uint32_t imageCount;
		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
		m_SwapchainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());

		m_SwapchainImagesExtent = framebufferExtent;

		CreateImageViews();
		CreateFramebuffers();
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
			assert(vkCreateImageView(m_Device, &imageViewCreateInfo, nullptr, &m_SwapchainImageViews.at(i)) == VK_SUCCESS);
		}
	}

	void Application::CreateRenderPass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = m_SurfaceFormat.format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		VkAttachmentReference attachmentRef{};
		attachmentRef.attachment = 0;
		attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescription{};
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &attachmentRef;

		VkRenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = 1;
		renderPassCreateInfo.pAttachments = &colorAttachment;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpassDescription;

		VkSubpassDependency subpassDependency{};
		subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		subpassDependency.dstSubpass = 0;
		subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.srcAccessMask = 0;
		subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		renderPassCreateInfo.dependencyCount = 1;
		renderPassCreateInfo.pDependencies = &subpassDependency;

		assert(vkCreateRenderPass(m_Device, &renderPassCreateInfo, nullptr, &m_RenderPass) == VK_SUCCESS);
	}

	void Application::CreateFramebuffers()
	{
		m_Framebuffers.resize(m_SwapchainImageViews.size());

		for (size_t i = 0; i < m_SwapchainImageViews.size(); i++)
		{
			VkFramebufferCreateInfo framebufferCreateInfo{};
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.attachmentCount = 1;
			framebufferCreateInfo.pAttachments = &m_SwapchainImageViews.at(i);
			framebufferCreateInfo.renderPass = m_RenderPass;
			framebufferCreateInfo.width = m_SwapchainImagesExtent.width;
			framebufferCreateInfo.height = m_SwapchainImagesExtent.height;
			framebufferCreateInfo.layers = 1;
			assert(vkCreateFramebuffer(m_Device, &framebufferCreateInfo, nullptr, &m_Framebuffers.at(i)) == VK_SUCCESS);
		}
	}

	VkCommandPool Application::CreateCommandPool()
	{
		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
		VkCommandPoolCreateInfo commandPoolCreateInfo{};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		commandPoolCreateInfo.queueFamilyIndex = indices.GraphicsFamily.value();

		VkCommandPool commandPool;
		assert(vkCreateCommandPool(m_Device, &commandPoolCreateInfo, nullptr, &commandPool) == VK_SUCCESS);
		return commandPool;
	}

	VkCommandBuffer Application::AllocateCommandBuffer(VkCommandPool commandPool)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandBufferCount = 1;
		commandBufferAllocateInfo.commandPool = commandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		VkCommandBuffer commandBuffer;
		assert(vkAllocateCommandBuffers(m_Device, &commandBufferAllocateInfo, &commandBuffer) == VK_SUCCESS);
		return commandBuffer;
	}

	void Application::RecordCommandBuffer(uint32_t imageIndex, VkCommandBuffer commandBuffer)
	{
		VkCommandBufferBeginInfo commandBufferBeginInfo{};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		
		assert(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) == VK_SUCCESS);

		OPTICK_GPU_CONTEXT(commandBuffer);
		OPTICK_GPU_EVENT("Draw Triangle");

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = m_RenderPass;
		renderPassBeginInfo.framebuffer = m_Framebuffers.at(imageIndex);
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = m_SwapchainImagesExtent;
		VkClearValue clearValues{};
		clearValues.color = { { 0.1f, 0.1f, 0.1f, 1.0f } };
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearValues;
		{
			OPTICK_GPU_EVENT("Begin render pass");
			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		}
		{
			OPTICK_GPU_EVENT("Bind graphics pipeline");
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
		}

		{
			OPTICK_GPU_EVENT("Set viewport state");
			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = m_SwapchainImagesExtent.width;
			viewport.height = m_SwapchainImagesExtent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		}

		{
			OPTICK_GPU_EVENT("Set scissor state");
			VkRect2D scissorRect{};
			scissorRect.offset = { 0, 0 };
			scissorRect.extent = m_SwapchainImagesExtent;
			vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
		}
		
		{
			OPTICK_GPU_EVENT("Draw");
			vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		}

		{
			OPTICK_GPU_EVENT("End render pass");
			vkCmdEndRenderPass(commandBuffer);
		}

		assert(vkEndCommandBuffer(commandBuffer) == VK_SUCCESS);
	}

	void Application::CreateSyncObjects(
		VkSemaphore& swapchainImageAcquireSemaphore,
		VkSemaphore& queueReadySemaphore, VkFence& presentFence)
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo{};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		assert(vkCreateSemaphore(
			m_Device,
			&semaphoreCreateInfo,
			nullptr,
			&swapchainImageAcquireSemaphore) == VK_SUCCESS);
		assert(vkCreateSemaphore(
			m_Device,
			&semaphoreCreateInfo,
			nullptr,
			&queueReadySemaphore) == VK_SUCCESS);

		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

		assert(vkCreateFence(
			m_Device,
			&fenceCreateInfo,
			nullptr,
			&presentFence) == VK_SUCCESS);
	}

	void Application::CreatePerFrameObjects(uint32_t frameIndex)
	{
		PerFrameData& data = m_PerFrameData.at(frameIndex);
		data.CommandPool = CreateCommandPool();
		data.CommandBuffer = AllocateCommandBuffer(data.CommandPool);

		CreateSyncObjects(
			data.SwapchainImageAcquireSemaphore,
			data.QueueReadySemaphore,
			data.PresentFence);
	}

	void Application::DrawFrame()
	{
		OPTICK_EVENT();
		uint32_t imageIndex;
		PerFrameData& data = m_PerFrameData.at(m_FrameIndex);
		{
			{
				OPTICK_GPU_EVENT("Acquire swapchain image");
				vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, data.SwapchainImageAcquireSemaphore, VK_NULL_HANDLE, &imageIndex);
			}

			vkResetCommandBuffer(data.CommandBuffer, 0);
			RecordCommandBuffer(imageIndex, data.CommandBuffer);
			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

			VkPipelineStageFlags pipelineStageFlags = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &data.SwapchainImageAcquireSemaphore;
			submitInfo.pWaitDstStageMask = &pipelineStageFlags;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &data.CommandBuffer;
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &data.QueueReadySemaphore;
			{
				OPTICK_GPU_EVENT("Queue submit");
				assert(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, data.PresentFence) == VK_SUCCESS);
			}

			VkPresentInfoKHR presentInfo{};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = &data.QueueReadySemaphore;
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &m_Swapchain;
			presentInfo.pImageIndices = &imageIndex;

			{
				OPTICK_GPU_EVENT("Queue Present KHR");
				vkQueuePresentKHR(m_PresentQueue, &presentInfo);
			}

			m_FrameIndex = (m_FrameIndex + 1) % m_PerFrameData.size();

			{
				OPTICK_EVENT("Wait and reset fence");
				vkWaitForFences(m_Device, 1, &data.PresentFence, VK_TRUE, UINT64_MAX);
				vkResetFences(m_Device, 1, &data.PresentFence);
			}
		}
	}

	VkShaderModule Application::CreateShader(const std::vector<char>& shaderData) 
	{
		VkShaderModuleCreateInfo shaderModuleCreateInfo{};
		shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleCreateInfo.codeSize = shaderData.size();
		shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderData.data());

		VkShaderModule shaderModule;
		assert(vkCreateShaderModule(m_Device, &shaderModuleCreateInfo, nullptr, &shaderModule) == VK_SUCCESS);
		return shaderModule;
	}

	void Application::DestroySwapchain(VkSwapchainKHR swapchain)
	{
		for (const auto& framebuffer : m_Framebuffers)
			vkDestroyFramebuffer(m_Device, framebuffer, nullptr);

		for (const auto& imageView : m_SwapchainImageViews)
			vkDestroyImageView(m_Device, imageView, nullptr);

		vkDestroySwapchainKHR(m_Device, swapchain, nullptr);
	}

	void Application::OnResize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) 
		{
			m_Minimized = true;
			return;
		}

		m_Minimized = false;
		CreateSwapchain();
	}

	void Application::CreateGraphicsPipeline()
	{
		std::vector<char> vertexShaderData = ReadShaderFile("assets/shaders/bin/BasicVert.spv");
		std::vector<char> fragmentShaderData = ReadShaderFile("assets/shaders/bin/BasicFrag.spv");

		VkShaderModule vertexShader = CreateShader(vertexShaderData);
		VkShaderModule fragmentShader = CreateShader(fragmentShaderData);

		VkPipelineShaderStageCreateInfo pipelineShaderVertexStageCreateInfo{};
		pipelineShaderVertexStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineShaderVertexStageCreateInfo.module = vertexShader;
		pipelineShaderVertexStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		pipelineShaderVertexStageCreateInfo.pName = "main";

		VkPipelineShaderStageCreateInfo pipelineShaderFragmentStageCreateInfo{};
		pipelineShaderFragmentStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineShaderFragmentStageCreateInfo.module = fragmentShader;
		pipelineShaderFragmentStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		pipelineShaderFragmentStageCreateInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { pipelineShaderVertexStageCreateInfo, pipelineShaderFragmentStageCreateInfo };

		constexpr size_t dynamicStateSize = 2;
		std::array<VkDynamicState, dynamicStateSize> dynamicStates
		{
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_VIEWPORT
		};

		VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
		pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
		pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;

		VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
		pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		pipelineDynamicStateCreateInfo.dynamicStateCount = dynamicStateSize;
		pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

		VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
		pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;
		pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{};
		pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

		VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
		pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
		pipelineColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{};
		pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		pipelineViewportStateCreateInfo.scissorCount = 1;
		pipelineViewportStateCreateInfo.viewportCount = 1;

		/* color blending op
		VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState2{};
		pipelineColorBlendAttachmentState2.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		pipelineColorBlendAttachmentState2.blendEnable = VK_TRUE;
		pipelineColorBlendAttachmentState2.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		pipelineColorBlendAttachmentState2.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		pipelineColorBlendAttachmentState2.colorBlendOp = VK_BLEND_OP_ADD;
		pipelineColorBlendAttachmentState2.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		pipelineColorBlendAttachmentState2.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		pipelineColorBlendAttachmentState2.alphaBlendOp = VK_BLEND_OP_ADD;
		*/

		VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{};
		pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		pipelineColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
		pipelineColorBlendStateCreateInfo.attachmentCount = 1;
		pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

		assert(vkCreatePipelineLayout(m_Device, &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout) == VK_SUCCESS);

		VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
		graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		graphicsPipelineCreateInfo.layout = m_PipelineLayout;
		graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
		graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
		graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
		graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
		graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
		graphicsPipelineCreateInfo.pVertexInputState = &pipelineVertexInputStateCreateInfo;
		graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;

		graphicsPipelineCreateInfo.stageCount = 2;
		graphicsPipelineCreateInfo.pStages = shaderStages;

		graphicsPipelineCreateInfo.renderPass = m_RenderPass;
		graphicsPipelineCreateInfo.subpass = 0;

		assert(vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_Pipeline) == VK_SUCCESS);


		vkDestroyShaderModule(m_Device, fragmentShader, nullptr);
		vkDestroyShaderModule(m_Device, vertexShader, nullptr);
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
