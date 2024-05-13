#include "Framebuffer.h"
#include "RendererContext.h"

#include <cassert>

namespace LearningVulkan 
{
	LearningVulkan::Framebuffer::Framebuffer(VkRenderPass renderPass, VkImageView attachment, uint32_t width, uint32_t height)
		: m_RenderPass(renderPass), m_Attachment(attachment), m_Width(width), m_Height(height)
	{
		Create();
	}

	LearningVulkan::Framebuffer::~Framebuffer()
	{
		Destroy();
	}

	void Framebuffer::Resize(VkImageView attachment, uint32_t width, uint32_t height)
	{
		m_Attachment = attachment;
		m_Width = width; m_Height = height;
		Destroy();
		Create();
	}

	void Framebuffer::Create()
	{
		VkFramebufferCreateInfo framebufferCreateInfo{};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.attachmentCount = 1;
		framebufferCreateInfo.pAttachments = &m_Attachment;
		framebufferCreateInfo.renderPass = m_RenderPass;
		framebufferCreateInfo.width = m_Width;
		framebufferCreateInfo.height = m_Height;
		framebufferCreateInfo.layers = 1;

		assert(vkCreateFramebuffer(RendererContext::GetLogicalDevice()->GetVulkanDevice(),
			&framebufferCreateInfo, nullptr, &m_Framebuffer) == VK_SUCCESS);
	}

	void Framebuffer::Destroy()
	{
		vkDestroyFramebuffer(RendererContext::GetLogicalDevice()->GetVulkanDevice(), m_Framebuffer, nullptr);
	}
}
