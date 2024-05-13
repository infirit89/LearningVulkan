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
		Swapchain* swapchain = m_RenderContext->GetSwapchain();
		m_PerFrameData.resize(swapchain->GetImageViews().size());
		for (uint32_t i = 0; i < swapchain->GetImageViews().size(); i++)
			CreatePerFrameObjects(i);

		CreateGraphicsPipeline();
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

		const VkExtent2D& swapchainExtent = m_RenderContext->GetSwapchain()->GetExtent();
		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = m_RenderContext->GetRenderPass();
		renderPassBeginInfo.framebuffer = m_RenderContext->GetFramebuffers().at(imageIndex)->GetVulkanFramebuffer();
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = swapchainExtent;
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
			viewport.width = swapchainExtent.width;
			viewport.height = swapchainExtent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		}

		{
			OPTICK_GPU_EVENT("Set scissor state");
			VkRect2D scissorRect{};
			scissorRect.offset = { 0, 0 };
			scissorRect.extent = swapchainExtent;
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
				m_RenderContext->GetSwapchain()->AcquireNextImage(data.SwapchainImageAcquireSemaphore, imageIndex);
			}

			std::cout << imageIndex << '\n';
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

			{
				OPTICK_GPU_EVENT("Queue Present KHR");
				m_RenderContext->GetSwapchain()->Present(data.QueueReadySemaphore, imageIndex);
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

	void Application::OnResize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) 
		{
			m_Minimized = true;
			return;
		}

		m_Minimized = false;
		m_RenderContext->Resize(width, height);
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

		graphicsPipelineCreateInfo.renderPass = m_RenderContext->GetRenderPass();
		graphicsPipelineCreateInfo.subpass = 0;

		assert(vkCreateGraphicsPipelines(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_Pipeline) == VK_SUCCESS);


		vkDestroyShaderModule(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), fragmentShader, nullptr);
		vkDestroyShaderModule(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), vertexShader, nullptr);
	}
}
