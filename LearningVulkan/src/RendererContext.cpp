#include "RendererContext.h"
#include "VulkanUtils.h"
#include "Application.h"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <array>
#include <set>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <fstream>
#include <filesystem>

#include "Vertex.h"

namespace LearningVulkan
{
	namespace
	{
		VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
		{
			switch (messageSeverity)
			{
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
				std::cerr << "validation trace: " << callbackData->pMessage << '\n';
				break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
				std::cerr << "validation info: " << callbackData->pMessage << '\n';
				break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
				std::cerr << "validation warning: " << callbackData->pMessage << '\n';
				break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
				std::cerr << "validation error: " << callbackData->pMessage << '\n';
				break;
			}

			return VK_FALSE;
		}

		void SetupDebugUtilsMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& messengerCreateInfo)
		{
			messengerCreateInfo.pfnUserCallback = DebugCallback;
			messengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			messengerCreateInfo.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

			messengerCreateInfo.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		}

		VkResult CreateDebugUtilsMessanger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* messangerCreateInfo,
			const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* debugMessanger)
		{
			PFN_vkCreateDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
			assert(func);

			return func(instance, messangerCreateInfo, allocator, debugMessanger);
		}

		void DestroyDebugUtilsMessanger(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* allocator)
		{
			PFN_vkDestroyDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
				vkGetInstanceProcAddr(
					instance, "vkDestroyDebugUtilsMessengerEXT"));
			assert(func);

			func(instance, debugMessenger, allocator);
		}

		uint32_t FindMemoryType(uint32_t memoryMask, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice)
		{
			VkPhysicalDeviceMemoryProperties memoryProperties;
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

			for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
				if (memoryMask & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
					return i;

			assert(false);
		}
	}

	VkInstance RendererContext::m_Instance;
	VkSurfaceKHR RendererContext::m_Surface;
	LogicalDevice* RendererContext::m_LogicalDevice;

	RendererContext::RendererContext(std::string_view applicationName)
	{
		CreateVulkanInstance(applicationName);
		SetupDebugMessenger();
		CreateSurface();

		m_PhysicalDevice = PhysicalDevice::GetSuitablePhysicalDevice();
		assert(m_PhysicalDevice != nullptr);
		m_LogicalDevice = m_PhysicalDevice->CreateLogicalDevice();

		const Window* window = Application::Get()->GetWindow();
		m_Swapchain = new Swapchain(m_LogicalDevice, window->GetWidth(), window->GetHeight(), PresentMode::Mailbox);
		CreateRenderPass();

		m_Framebuffers.reserve(m_Swapchain->GetImageViews().size());
		for (size_t i = 0; i < m_Swapchain->GetImageViews().size(); i++)
		{
			const VkExtent2D& extent = m_Swapchain->GetExtent();
			Framebuffer* framebuffer = new Framebuffer(m_RenderPass, m_Swapchain->GetImageViews().at(i), 
				extent.width, extent.height);
			m_Framebuffers.push_back(framebuffer);
		}

		m_PerFrameData.resize(m_Swapchain->GetImageViews().size());
		for (size_t i = 0; i < m_Swapchain->GetImageViews().size(); ++i)
			CreatePerFrameObjects(i);

		CreateGraphicsPipeline();
		CreateVertexBuffer();
	}

	RendererContext::~RendererContext()
	{
		for (const PerFrameData& data : m_PerFrameData)
		{
			vkDestroySemaphore(m_LogicalDevice->GetVulkanDevice(), data.SwapchainImageAcquireSemaphore, nullptr);
			vkDestroySemaphore(m_LogicalDevice->GetVulkanDevice(), data.QueueReadySemaphore, nullptr);
			vkDestroyFence(m_LogicalDevice->GetVulkanDevice(), data.PresentFence, nullptr);
			vkFreeCommandBuffers(m_LogicalDevice->GetVulkanDevice(), data.CommandPool, 1, &data.CommandBuffer);
			vkDestroyCommandPool(m_LogicalDevice->GetVulkanDevice(), data.CommandPool, nullptr);
		}

		vkDestroyPipeline(m_LogicalDevice->GetVulkanDevice(), m_Pipeline, nullptr);
		vkDestroyPipelineLayout(m_LogicalDevice->GetVulkanDevice(), m_PipelineLayout, nullptr);
		
		for (const auto& framebuffer : m_Framebuffers)
			delete framebuffer;

		vkDestroyRenderPass(m_LogicalDevice->GetVulkanDevice(), m_RenderPass, nullptr);

		delete m_Swapchain;

		vkDestroyBuffer(m_LogicalDevice->GetVulkanDevice(), m_VertexBuffer, nullptr);
		vkFreeMemory(m_LogicalDevice->GetVulkanDevice(), m_VertexBufferMemory, nullptr);

		delete m_LogicalDevice;

		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

		delete m_PhysicalDevice;

		DestroyDebugUtilsMessanger(m_Instance, m_DebugMessenger, nullptr);
		vkDestroyInstance(m_Instance, nullptr);
	}

	void RendererContext::Resize(uint32_t width, uint32_t height) const
	{
		m_Swapchain->Recreate(width, height);
		uint32_t index = 0;
		for (const auto& framebuffer : m_Framebuffers) 
			framebuffer->Resize(m_Swapchain->GetImageViews().at(index++), width, height);
	}

	void RendererContext::CreateVulkanInstance(std::string_view applicationName)
	{
		VkApplicationInfo applicationInfo{};
		applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		applicationInfo.pApplicationName = applicationName.data();
		applicationInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo instanceCreateInfo{};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pApplicationInfo = &applicationInfo;

		// get the required instance extensions
		uint32_t extensionCount = 0;
		const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
		std::vector<const char*> extensions(requiredExtensions, requiredExtensions + extensionCount);

		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		instanceCreateInfo.enabledExtensionCount = extensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

		// there's an unavailable layer
		assert(CheckLayersAvailability() == true);

		// TODO: enable if we want to use the validation layer
		instanceCreateInfo.enabledLayerCount = VulkanUtils::Layers.size();
		instanceCreateInfo.ppEnabledLayerNames = VulkanUtils::Layers.data();

		VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
		SetupDebugUtilsMessengerCreateInfo(debugMessengerCreateInfo);
		instanceCreateInfo.pNext = &debugMessengerCreateInfo;

		vkCreateInstance(&instanceCreateInfo, nullptr, &m_Instance);
	}

	bool RendererContext::CheckLayersAvailability()
	{
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> supportedLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, supportedLayers.data());

		std::set<std::string> requiredLayers(VulkanUtils::Layers.begin(), VulkanUtils::Layers.end());

		for (const auto& layer : supportedLayers)
			requiredLayers.erase(layer.layerName);

		return requiredLayers.empty();
	}

	void RendererContext::SetupDebugMessenger()
	{
		VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
		SetupDebugUtilsMessengerCreateInfo(debugMessengerCreateInfo);
		assert(CreateDebugUtilsMessanger(m_Instance, &debugMessengerCreateInfo, nullptr, &m_DebugMessenger) == VK_SUCCESS);
	}

	void RendererContext::CreateSurface()
	{
		const Window* window = Application::Get()->GetWindow();
		assert(glfwCreateWindowSurface(m_Instance, window->GetNativeWindow(), nullptr, &m_Surface) == VK_SUCCESS);
	}

	void RendererContext::CreateRenderPass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = m_Swapchain->GetSurfaceFormat().format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		VkAttachmentReference attachmentRef;
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

		assert(vkCreateRenderPass(m_LogicalDevice->GetVulkanDevice(), &renderPassCreateInfo, nullptr, &m_RenderPass) == VK_SUCCESS);
	}

	VkCommandPool RendererContext::CreateCommandPool() const
	{
		const QueueFamilyIndices& queueFamilies = m_PhysicalDevice->GetQueueFamilyIndices();
		VkCommandPoolCreateInfo commandPoolCreateInfo{};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		commandPoolCreateInfo.queueFamilyIndex = queueFamilies.GraphicsFamily.value();

		VkCommandPool commandPool;
		assert(vkCreateCommandPool(m_LogicalDevice->GetVulkanDevice(), &commandPoolCreateInfo, nullptr, &commandPool) == VK_SUCCESS);
		return commandPool;
	}

	VkCommandBuffer RendererContext::AllocateCommandBuffer(VkCommandPool commandPool)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandBufferCount = 1;
		commandBufferAllocateInfo.commandPool = commandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		
		VkCommandBuffer commandBuffer;
		assert(vkAllocateCommandBuffers(m_LogicalDevice->GetVulkanDevice(), &commandBufferAllocateInfo, &commandBuffer) == VK_SUCCESS);
		return commandBuffer;
	}

	void RendererContext::RecordCommandBuffer(uint32_t imageIndex, VkCommandBuffer commandBuffer)
	{
		VkCommandBufferBeginInfo commandBufferInfo{};
		commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		assert(vkBeginCommandBuffer(commandBuffer, &commandBufferInfo) == VK_SUCCESS);

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.framebuffer = m_Framebuffers.at(imageIndex)->GetVulkanFramebuffer();
		renderPassInfo.renderPass = m_RenderPass;
		renderPassInfo.renderArea.extent = m_Swapchain->GetExtent();
		renderPassInfo.renderArea.offset = { 0, 0 };

		VkClearValue clearColor{};
		clearColor.color = { 0.1f, 0.1f, 0.1f, 1.0f };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

		VkDeviceSize deviceSizes[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_VertexBuffer, deviceSizes);

		VkViewport viewport;
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = m_Swapchain->GetExtent().width;
		viewport.height = m_Swapchain->GetExtent().height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = m_Swapchain->GetExtent();
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		vkCmdDraw(commandBuffer, m_Vertices.size(), 1, 0, 0);

		vkCmdEndRenderPass(commandBuffer);
		vkEndCommandBuffer(commandBuffer);
	}

	void RendererContext::CreateSyncObjects(VkSemaphore& swapchainImageAcquireSemaphore, VkSemaphore& queueReadySemaphore, VkFence& presentFence)
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo{};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		assert(vkCreateSemaphore(m_LogicalDevice->GetVulkanDevice(), &semaphoreCreateInfo, nullptr, &swapchainImageAcquireSemaphore) == VK_SUCCESS);

		assert(vkCreateSemaphore(m_LogicalDevice->GetVulkanDevice(), &semaphoreCreateInfo, nullptr, &queueReadySemaphore) == VK_SUCCESS);

		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		assert(vkCreateFence(m_LogicalDevice->GetVulkanDevice(), &fenceCreateInfo, nullptr, &presentFence) == VK_SUCCESS);
	}

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

	void RendererContext::CreatePerFrameObjects(uint32_t frameIndex)
	{
		PerFrameData& data = m_PerFrameData.at(frameIndex);
		data.CommandPool = CreateCommandPool();
		data.CommandBuffer = AllocateCommandBuffer(data.CommandPool);
		CreateSyncObjects(
			data.SwapchainImageAcquireSemaphore,
			data.QueueReadySemaphore,
			data.PresentFence);
	}

	void RendererContext::DrawFrame()
	{
		const PerFrameData& currentFrameData = m_PerFrameData.at(m_FrameIndex);
		assert(vkWaitForFences(m_LogicalDevice->GetVulkanDevice(), 1, &currentFrameData.PresentFence, VK_TRUE, UINT64_MAX) == VK_SUCCESS);
		vkResetFences(m_LogicalDevice->GetVulkanDevice(), 1, &currentFrameData.PresentFence);

		uint32_t imageIndex;
		m_Swapchain->AcquireNextImage(currentFrameData.SwapchainImageAcquireSemaphore, imageIndex);

		vkResetCommandBuffer(currentFrameData.CommandBuffer, 0);
		RecordCommandBuffer(imageIndex, currentFrameData.CommandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &currentFrameData.CommandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &currentFrameData.QueueReadySemaphore;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &currentFrameData.SwapchainImageAcquireSemaphore;
		VkPipelineStageFlags waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.pWaitDstStageMask = &waitStages;

		assert(vkQueueSubmit(m_LogicalDevice->GetGraphicsQueue(), 1, &submitInfo, currentFrameData.PresentFence) == VK_SUCCESS);
		m_Swapchain->Present(currentFrameData.QueueReadySemaphore, imageIndex);

		m_FrameIndex = (m_FrameIndex + 1) % m_PerFrameData.size();
	}

	void RendererContext::CreateGraphicsPipeline()
	{
		VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
		graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		graphicsPipelineCreateInfo.renderPass = m_RenderPass;

		std::vector<char> vertexShaderData = ReadShaderFile("assets/shaders/bin/BasicVert.spv");
		std::vector<char> fragmentShaderData = ReadShaderFile("assets/shaders/bin/BasicFrag.spv");

		VkShaderModule vertexShaderModule = CreateShader(vertexShaderData);
		VkShaderModule fragmentShaderModule = CreateShader(fragmentShaderData);

		{
			VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo{};
			vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertexShaderStageCreateInfo.module = vertexShaderModule;
			vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vertexShaderStageCreateInfo.pName = "main";

			VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo{};
			fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			fragmentShaderStageCreateInfo.module = fragmentShaderModule;
			fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragmentShaderStageCreateInfo.pName = "main";

			std::array shaderStages =
			{
				vertexShaderStageCreateInfo,
				fragmentShaderStageCreateInfo
			};

			graphicsPipelineCreateInfo.pStages = shaderStages.data();
			graphicsPipelineCreateInfo.stageCount = shaderStages.size();
		}


		{
			VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
			vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

			vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
			VkVertexInputBindingDescription vertexBindingDescription = Vertex::GetBindingDescription();
			vertexInputCreateInfo.pVertexBindingDescriptions = &vertexBindingDescription;

			std::array vertexAttributeDescription = Vertex::GetAttributeDescriptions();
			vertexInputCreateInfo.vertexAttributeDescriptionCount = vertexAttributeDescription.size();
			vertexInputCreateInfo.pVertexAttributeDescriptions = vertexAttributeDescription.data();

			graphicsPipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
		}

		{
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
			inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

			graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
		}

		{
			std::array dynamicStates =
			{
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR
			};

			VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
			dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicStateCreateInfo.dynamicStateCount = dynamicStates.size();
			dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();
			
			graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
		}

		{
			VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
			rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
			rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
			rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
			rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
			rasterizationStateCreateInfo.lineWidth = 1.0f;
			rasterizationStateCreateInfo.depthBiasClamp = VK_FALSE;

			graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
		}

		{
			VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
			viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			// not setting the actual viewport and scissor states just how many there will be
			// that's because both are dynamic and will be specified later
			viewportStateCreateInfo.scissorCount = 1;
			viewportStateCreateInfo.viewportCount = 1;

			graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
		}

		{
			VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
			rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

			// if true, then the fragments outside the depth range (the near and far planes) will be clamped, else they will be discarded
			// using this requires a gpu feature
			rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
			rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;

			rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizationStateCreateInfo.lineWidth = 1.0f;
			rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
			rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
			rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;

			graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
		}

		{
			VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
			multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

			graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
		}

		{
			// TODO:
			graphicsPipelineCreateInfo.pDepthStencilState = nullptr;
		}

		{
			VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};

			// specifies to which color components to write to
			colorBlendAttachmentState.colorWriteMask =
				VK_COLOR_COMPONENT_R_BIT
				| VK_COLOR_COMPONENT_G_BIT
				| VK_COLOR_COMPONENT_B_BIT
				| VK_COLOR_COMPONENT_A_BIT;

			colorBlendAttachmentState.blendEnable = VK_FALSE;
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


			VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
			colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendStateCreateInfo.attachmentCount = 1;
			colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
			colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;

			graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
		}

		{
			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
			pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

			assert(vkCreatePipelineLayout(m_LogicalDevice->GetVulkanDevice(), &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout) == VK_SUCCESS);

			graphicsPipelineCreateInfo.layout = m_PipelineLayout;
		}

		{
			graphicsPipelineCreateInfo.renderPass = m_RenderPass;
			graphicsPipelineCreateInfo.subpass = 0;
		}

		assert(vkCreateGraphicsPipelines(m_LogicalDevice->GetVulkanDevice(), VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_Pipeline) == VK_SUCCESS);

		vkDestroyShaderModule(m_LogicalDevice->GetVulkanDevice(), vertexShaderModule, nullptr);
		vkDestroyShaderModule(m_LogicalDevice->GetVulkanDevice(), fragmentShaderModule, nullptr);
	}

	VkShaderModule RendererContext::CreateShader(const std::vector<char>& shaderData)
	{
		VkShaderModuleCreateInfo shaderModuleCreateInfo{};
		shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleCreateInfo.codeSize = shaderData.size();
		shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderData.data());

		VkShaderModule shaderModule;
		assert(vkCreateShaderModule(m_LogicalDevice->GetVulkanDevice(), &shaderModuleCreateInfo, nullptr, &shaderModule) == VK_SUCCESS);
		return shaderModule;
	}

	void RendererContext::CreateVertexBuffer()
	{
		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferCreateInfo.size = sizeof(Vertex) * m_Vertices.size();

		assert(vkCreateBuffer(m_LogicalDevice->GetVulkanDevice(), &bufferCreateInfo, nullptr, &m_VertexBuffer) == VK_SUCCESS);

		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(m_LogicalDevice->GetVulkanDevice(), m_VertexBuffer, &memoryRequirements);

		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = FindMemoryType(
			memoryRequirements.memoryTypeBits, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			m_PhysicalDevice->GetPhysicalDevice());


		assert(vkAllocateMemory(m_LogicalDevice->GetVulkanDevice(), &allocateInfo, nullptr, &m_VertexBufferMemory) == VK_SUCCESS);

		assert(vkBindBufferMemory(m_LogicalDevice->GetVulkanDevice(), m_VertexBuffer, m_VertexBufferMemory, 0) == VK_SUCCESS);

		void* data;
		vkMapMemory(m_LogicalDevice->GetVulkanDevice(), m_VertexBufferMemory, 0, memoryRequirements.size, 0, &data);
		memcpy(data, m_Vertices.data(), sizeof(Vertex) * m_Vertices.size());
		vkUnmapMemory(m_LogicalDevice->GetVulkanDevice(), m_VertexBufferMemory);
	}
}
