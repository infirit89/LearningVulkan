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
	Application* Application::m_Instance;

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
		m_Instance = this;
		m_Window = new Window(640, 480, "i cum hard uwu");

		m_Window->SetResizeFn(std::bind(&Application::OnResize, this, std::placeholders::_1, std::placeholders::_2));
		SetupRenderer();
	}

	Application::~Application()
	{
		for (const auto& data : m_PerFrameData)
		{
			vkDestroySemaphore(
				m_RenderContext->GetLogicalDevice()->GetVulkanDevice(),
				data.SwapchainImageAcquireSemaphore,
				nullptr);
			vkDestroySemaphore(
				m_RenderContext->GetLogicalDevice()->GetVulkanDevice(),
				data.QueueReadySemaphore,
				nullptr);
			vkDestroyFence(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), data.PresentFence, nullptr);
			vkFreeCommandBuffers(
				m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), data.CommandPool, 1, &data.CommandBuffer);
			vkDestroyCommandPool(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), data.CommandPool, nullptr);
		}

		vkDestroyPipeline(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), m_Pipeline, nullptr);
		vkDestroyPipelineLayout(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), m_PipelineLayout, nullptr);

		for (const auto& framebuffer : m_Framebuffers)
			vkDestroyFramebuffer(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), framebuffer, nullptr);

		vkDestroyRenderPass(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), m_RenderPass, nullptr);

		for (const auto& imageView : m_SwapchainImageViews)
			vkDestroyImageView(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), imageView, nullptr);

		vkDestroySwapchainKHR(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), m_Swapchain, nullptr);
		//vkDestroySurfaceKHR(m_RenderContext->GetVulkanInstance(), m_Surface, nullptr);
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

		m_RenderContext->GetLogicalDevice()->WaitIdle();
	}

	void Application::SetupRenderer()
	{
		m_RenderContext = new RendererContext("Learning Vulkan");
		//m_PhysicalDevice = 
		//SetupLogicalDevice();
		m_SwapchainDetails = m_RenderContext->GetPhysicalDevice()->QuerySwapChainSupport(m_RenderContext->GetPhysicalDevice()->GetPhysicalDevice());
		m_SurfaceFormat = ChooseCorrectSurfaceFormat(m_SwapchainDetails.SurfaceFormats);
		CreateRenderPass();
		CreateSwapchain();
		m_PerFrameData.resize(m_SwapchainImages.size());
		for (uint32_t i = 0; i < m_SwapchainImages.size(); i++)
			CreatePerFrameObjects(i);

		CreateGraphicsPipeline();
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
		m_SwapchainDetails = m_RenderContext->GetPhysicalDevice()->QuerySwapChainSupport(m_RenderContext->GetPhysicalDevice()->GetPhysicalDevice());
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
		swapchainCreateInfo.surface = m_RenderContext->GetVulkanSurface();
		swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchainCreateInfo.imageArrayLayers = 1;
		swapchainCreateInfo.minImageCount = minImageCount;

		const QueueFamilyIndices& queueFamilyIndices = m_RenderContext->GetPhysicalDevice()->GetQueueFamilyIndices();

		//uint32_t indices[] = { queueFamilyIndices.GraphicsFamily.value(), queueFamilyIndices.PresentationFamily.value() };

		/*if (queueFamilyIndices.GraphicsFamily != queueFamilyIndices.PresentationFamily) 
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
		}*/

		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

		swapchainCreateInfo.preTransform = m_SwapchainDetails.SurfaceCapabilities.currentTransform;
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.clipped = VK_TRUE;
		swapchainCreateInfo.oldSwapchain = oldSwapchain;

		assert(vkCreateSwapchainKHR(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), &swapchainCreateInfo, nullptr, &m_Swapchain) == VK_SUCCESS);

		if (oldSwapchain != VK_NULL_HANDLE)
			DestroySwapchain(oldSwapchain);

		uint32_t imageCount;
		vkGetSwapchainImagesKHR(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), m_Swapchain, &imageCount, nullptr);
		m_SwapchainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), m_Swapchain, &imageCount, m_SwapchainImages.data());

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
			assert(vkCreateImageView(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), &imageViewCreateInfo, nullptr, &m_SwapchainImageViews.at(i)) == VK_SUCCESS);
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

		assert(vkCreateRenderPass(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), &renderPassCreateInfo, nullptr, &m_RenderPass) == VK_SUCCESS);
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
			assert(vkCreateFramebuffer(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), &framebufferCreateInfo, nullptr, &m_Framebuffers.at(i)) == VK_SUCCESS);
		}
	}

	VkCommandPool Application::CreateCommandPool()
	{
		const QueueFamilyIndices& indices = m_RenderContext->GetPhysicalDevice()->GetQueueFamilyIndices();
		VkCommandPoolCreateInfo commandPoolCreateInfo{};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		commandPoolCreateInfo.queueFamilyIndex = indices.GraphicsFamily.value();

		VkCommandPool commandPool;
		assert(vkCreateCommandPool(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), &commandPoolCreateInfo, nullptr, &commandPool) == VK_SUCCESS);
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
		assert(vkAllocateCommandBuffers(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), &commandBufferAllocateInfo, &commandBuffer) == VK_SUCCESS);
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
			m_RenderContext->GetLogicalDevice()->GetVulkanDevice(),
			&semaphoreCreateInfo,
			nullptr,
			&swapchainImageAcquireSemaphore) == VK_SUCCESS);
		assert(vkCreateSemaphore(
			m_RenderContext->GetLogicalDevice()->GetVulkanDevice(),
			&semaphoreCreateInfo,
			nullptr,
			&queueReadySemaphore) == VK_SUCCESS);

		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

		assert(vkCreateFence(
			m_RenderContext->GetLogicalDevice()->GetVulkanDevice(),
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
				vkAcquireNextImageKHR(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), m_Swapchain, UINT64_MAX, data.SwapchainImageAcquireSemaphore, VK_NULL_HANDLE, &imageIndex);
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
				assert(vkQueueSubmit(m_RenderContext->GetLogicalDevice()->GetGraphicsQueue(), 1, &submitInfo, data.PresentFence) == VK_SUCCESS);
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
				//vkQueuePresentKHR(m_PresentQueue, &presentInfo);
				vkQueuePresentKHR(m_RenderContext->GetLogicalDevice()->GetGraphicsQueue(), &presentInfo);
			}

			m_FrameIndex = (m_FrameIndex + 1) % m_PerFrameData.size();

			{
				OPTICK_EVENT("Wait and reset fence");
				vkWaitForFences(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), 1, &data.PresentFence, VK_TRUE, UINT64_MAX);
				vkResetFences(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), 1, &data.PresentFence);
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
		assert(vkCreateShaderModule(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), &shaderModuleCreateInfo, nullptr, &shaderModule) == VK_SUCCESS);
		return shaderModule;
	}

	void Application::DestroySwapchain(VkSwapchainKHR swapchain)
	{
		for (const auto& framebuffer : m_Framebuffers)
			vkDestroyFramebuffer(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), framebuffer, nullptr);

		for (const auto& imageView : m_SwapchainImageViews)
			vkDestroyImageView(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), imageView, nullptr);

		vkDestroySwapchainKHR(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), swapchain, nullptr);
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

		assert(vkCreatePipelineLayout(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout) == VK_SUCCESS);

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

		assert(vkCreateGraphicsPipelines(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_Pipeline) == VK_SUCCESS);


		vkDestroyShaderModule(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), fragmentShader, nullptr);
		vkDestroyShaderModule(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), vertexShader, nullptr);
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
