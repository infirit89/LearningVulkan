#pragma once

#include <vulkan/vulkan.h>

namespace LearningVulkan 
{
    struct ImageCreateInfo
    {
        uint32_t Width, Height;
        VkFormat Format;
        VkImageTiling Tiling;
        VkImageUsageFlags Usage;
        VkMemoryPropertyFlags MemoryProperties;
        VkImageAspectFlags AspectFlags;
    };

    class Image
    {
    public:
        Image(const ImageCreateInfo& imageCreateInfo);
        ~Image();

        Image(const Image& other) = delete;
        Image(Image&& other) = delete;
        Image& operator=(const Image& other) = delete;

        VkImage GetVulkanImage() const { return m_Image; }
        VkImageView GetVulkanImageView() const 
        { 
            return m_ImageView; 
        }

        VkDeviceMemory GetVulkanImageMemory() const 
        { 
            return m_ImageMemory; 
        }

        VkImageLayout GetCurrentVulkanLayout() const 
        { 
            return m_CurrentLayout; 
        }

        VkFormat GetFormat() const 
        { 
            return m_Format; 
        }
        
        void TransitionLayout(VkImageLayout newLayout);
    private:
        void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkMemoryPropertyFlags memoryProperties);
        void CreateView(VkFormat imageFormat, VkImageAspectFlags imageAspect);
    private:
        VkImage m_Image;
        VkImageView m_ImageView;
        VkDeviceMemory m_ImageMemory;
        uint32_t m_Width, m_Height;
        VkImageLayout m_CurrentLayout;
        VkFormat m_Format;
    };
}
