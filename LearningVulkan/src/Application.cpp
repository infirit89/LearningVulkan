#include "Application.h"

#include <cassert>

#include <vector>
#include <iostream>
#include <map>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <optick.h>

namespace LearningVulkan 
{
	Application* Application::m_Instance;

	Application::Application()
	{
		m_Instance = this;
		m_Window = new Window(640, 480, "i cum hard uwu");

		m_Window->SetResizeFn(std::bind_front(&Application::OnResize, this));
		SetupRenderer();
	}

	Application::~Application()
	{
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
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_RenderContext->GetGraphicsPipeline());
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

	void Application::DrawFrame()
	{
		OPTICK_EVENT();
		uint32_t imageIndex;
		const PerFrameData& data = m_RenderContext->GetPerFrameData(m_FrameIndex);
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

			m_FrameIndex = (m_FrameIndex + 1) % m_RenderContext->GetPerFrameDataSize();

			{
				OPTICK_EVENT("Wait and reset fence");
				vkWaitForFences(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), 1, &data.PresentFence, VK_TRUE, UINT64_MAX);
				vkResetFences(m_RenderContext->GetLogicalDevice()->GetVulkanDevice(), 1, &data.PresentFence);
			}
		}
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
}
