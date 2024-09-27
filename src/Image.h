#pragma once

#include "vulkan/vulkan_core.h"
#include <vulkan/vulkan.h>

namespace LearningVulkan 
{
    class Image
    {
    public:
        Image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkMemoryPropertyFlags memoryProperties);
        ~Image();

        Image(const Image& other) = delete;
        Image(Image&& other) = delete;
        Image& operator=(const Image& other) = delete;	

        VkImage GetVulkanImage() const { return m_Image; }
        VkImageView GetVulkanImageView() const { return m_ImageView; }
        VkDeviceMemory GetVulkanImageMemory() const { return m_ImageMemory; }	
        VkImageLayout GetCurrentVulkanLayout() const { return m_CurrentLayout; }
        
        void TransitionLayout(VkImageLayout newLayout);
    private:
        void Create(uint32_t width, uint32_t height, VkFormat format, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkMemoryPropertyFlags memoryProperties);
    private:
        VkImage m_Image;
        VkImageView m_ImageView;
        VkDeviceMemory m_ImageMemory;
        uint32_t m_Width, m_Height;
        VkImageLayout m_CurrentLayout;
    };
}
