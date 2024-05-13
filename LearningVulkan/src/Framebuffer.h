#pragma once

#include <vulkan/vulkan.h>

namespace LearningVulkan 
{
	class Framebuffer
	{
	public:
		Framebuffer(VkRenderPass renderPass, VkImageView attachment, uint32_t width, uint32_t height);
		~Framebuffer();

		void Resize(VkImageView attachment, uint32_t width, uint32_t height);
		VkFramebuffer GetVulkanFramebuffer() const { return m_Framebuffer; }

	private:
		void Create();
		void Destroy();

	private:
		VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
		VkImageView m_Attachment = VK_NULL_HANDLE;
		VkRenderPass m_RenderPass;
		uint32_t m_Width, m_Height;
	};
}
