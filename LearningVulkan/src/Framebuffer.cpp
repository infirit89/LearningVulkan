#include "Framebuffer.h"
#include "RendererContext.h"

#include <cassert>

namespace LearningVulkan 
{
	LearningVulkan::Framebuffer::Framebuffer(VkRenderPass renderPass, const std::vector<VkImageView>& attachment, uint32_t width, uint32_t height)
		: m_Attachments(attachment), m_RenderPass(renderPass), m_Width(width), m_Height(height)
	{
		Create();
	}

	LearningVulkan::Framebuffer::~Framebuffer()
	{
		Destroy();
	}

	void Framebuffer::Resize(const std::vector<VkImageView>& attachment, uint32_t width, uint32_t height)
	{
		m_Attachments = attachment;
		m_Width = width; m_Height = height;
		Destroy();
		Create();
	}

	void Framebuffer::Create()
	{
		VkFramebufferCreateInfo framebufferCreateInfo{};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.attachmentCount = m_Attachments.size();
		framebufferCreateInfo.pAttachments = m_Attachments.data();
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
