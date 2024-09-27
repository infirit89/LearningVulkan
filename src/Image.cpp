#include "Image.h"
#include "LogicalDevice.h"
#include "RendererContext.h"
#include "vulkan/vulkan_core.h"

#include <vulkan/vulkan.h>

namespace LearningVulkan {
	Image::Image(uint32_t width, uint32_t height,
	      VkFormat format, VkImageTiling imageTiling,
	      VkImageUsageFlags imageUsage,
	      VkMemoryPropertyFlags memoryProperties)
		: m_Width(width), m_Height(height)
	{
		Create(width, height, format, imageTiling, imageUsage, memoryProperties);
	}

	Image::~Image()
	{
		LogicalDevice* logicalDevice = RendererContext::GetLogicalDevice();
		vkDestroyImageView(logicalDevice->GetVulkanDevice(), m_ImageView, nullptr);
		vkDestroyImage(logicalDevice->GetVulkanDevice(), m_Image, nullptr);
		vkFreeMemory(logicalDevice->GetVulkanDevice(), m_ImageMemory, nullptr);
	}

	void Image::TransitionLayout(VkImageLayout newLayout)
	{
		if(newLayout == m_CurrentLayout)
			return;

		VkImageMemoryBarrier memoryBarrier{};
		memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		memoryBarrier.image = m_Image;
		memoryBarrier.oldLayout = m_CurrentLayout;
		memoryBarrier.newLayout = newLayout;
		memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		memoryBarrier.subresourceRange.baseArrayLayer = 0;
		memoryBarrier.subresourceRange.baseMipLevel = 0;
		memoryBarrier.subresourceRange.layerCount = 1;
		memoryBarrier.subresourceRange.levelCount = 1;
		memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkCommandPool transientCommandPool = VK_NULL_HANDLE;
        LogicalDevice* logicalDevice = RendererContext::GetLogicalDevice();

		if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			transientCommandPool = 
				RendererContext::GetTransientGraphicsCommandPool();
		else
			transientCommandPool = 
				RendererContext::GetTransientTransferCommandPool();

        assert(transientCommandPool != VK_NULL_HANDLE);

		commandBuffer = 
			RendererContext::AllocateCommandBuffer(transientCommandPool);
		RendererContext::BeginCommandBuffer(
			commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		VkPipelineStageFlags srcStageFlags = 0;
		VkPipelineStageFlags dstStageFlags = 0;

		if(m_CurrentLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
			newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;

			memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		else if(m_CurrentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL 
			&& newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			srcStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

			memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}
		else
		{
			assert(false);
		}

		vkCmdPipelineBarrier(commandBuffer, srcStageFlags, dstStageFlags, 
			0, 0, nullptr, 
			0, nullptr, 1, &memoryBarrier);
		RendererContext::EndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		
        VkQueue transitionQueue = VK_NULL_HANDLE;
		if(newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            transitionQueue = logicalDevice->GetTransferQueue();
		else
            transitionQueue = logicalDevice->GetGraphicsQueue();

        logicalDevice->QueueSubmit(transitionQueue, 1, &submitInfo, VK_NULL_HANDLE);
        logicalDevice->QueueWaitIdle(transitionQueue);

        vkFreeCommandBuffers(logicalDevice->GetVulkanDevice(), transientCommandPool, 1, &commandBuffer);
	}

	void Image::Create(uint32_t width, uint32_t height, 
		VkFormat format, VkImageTiling imageTiling,
		VkImageUsageFlags imageUsage,
		VkMemoryPropertyFlags memoryProperties) 
	{
		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.format = format;
		imageCreateInfo.usage = imageUsage;
		imageCreateInfo.extent.width = width;
		imageCreateInfo.extent.height = height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.tiling = imageTiling;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		LogicalDevice* logicalDevice = 
			RendererContext::GetLogicalDevice();
		PhysicalDevice* physicalDevice = 
			logicalDevice->GetPhysicalDevice();
		const auto& queueFamilyIndices = 
			physicalDevice->GetQueueFamilyIndices();

		std::array queueFamilyIndicesArr = 
		{
			queueFamilyIndices.GraphicsFamily.value(),
			queueFamilyIndices.TransferFamily.value(),
		};

		imageCreateInfo.queueFamilyIndexCount = 
			queueFamilyIndicesArr.size();
		imageCreateInfo.pQueueFamilyIndices = 
			queueFamilyIndicesArr.data();

		assert(vkCreateImage(logicalDevice->GetVulkanDevice(), 
			&imageCreateInfo, nullptr, &m_Image) == VK_SUCCESS);
		
		VkMemoryRequirements imageMemoryRequirements;
		vkGetImageMemoryRequirements(logicalDevice->GetVulkanDevice(),
			       m_Image, &imageMemoryRequirements);

		VkMemoryAllocateInfo imageMemoryAllocateInfo{};
		imageMemoryAllocateInfo.sType = 
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		imageMemoryAllocateInfo.allocationSize = 
			imageMemoryRequirements.size;
		imageMemoryAllocateInfo.memoryTypeIndex = 
			physicalDevice->FindMemoryType(
				imageMemoryRequirements.memoryTypeBits,
				memoryProperties);

		assert(vkAllocateMemory(logicalDevice->GetVulkanDevice(),
			&imageMemoryAllocateInfo, nullptr, &m_ImageMemory) 
		== VK_SUCCESS);

		assert(vkBindImageMemory(logicalDevice->GetVulkanDevice(),
			m_Image, m_ImageMemory, 0) == VK_SUCCESS);
	}
}

