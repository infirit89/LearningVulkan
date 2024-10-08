#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "Image.h"

namespace LearningVulkan 
{
    enum class PresentMode
    {
        Mailbox,
        Fifo,
        Immediate
    };

    class LogicalDevice;
    class Swapchain
    {
    public:
        Swapchain(LogicalDevice* logicalDevice, uint32_t width, uint32_t height, PresentMode presentMode);
        ~Swapchain();

        const VkExtent2D& GetExtent() const;
        const std::vector<VkImageView>& GetImageViews() const;
        const VkSurfaceFormatKHR& GetSurfaceFormat() const;
        constexpr const VkSwapchainKHR& GetVulkanSwapchain() const;
        void Resize(uint32_t width, uint32_t height);
        void Present(VkSemaphore semaphore, uint32_t imageIndex);
        void AcquireNextImage(VkSemaphore imageAcquireSemaphore, uint32_t& imageIndex);
        constexpr Image* GetDepthImage() const;
        
    private:
        void Create();
        void Destroy(VkSwapchainKHR swapchain);
        constexpr const VkSurfaceFormatKHR& ChooseCorrectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats);
        constexpr VkPresentModeKHR ChooseSurfacePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
        constexpr VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
        void CreateDepthResources();

    private:
        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
        uint32_t m_Width, m_Height;
        PresentMode m_DesiredPresentMode;
        LogicalDevice* m_LogicalDevice;
        VkSurfaceFormatKHR m_SurfaceFormat;
        std::vector<VkImage> m_Images;
        std::vector<VkImageView> m_ImageViews;
        Image* m_DepthImage;
        VkExtent2D m_Extent;
    };
}
