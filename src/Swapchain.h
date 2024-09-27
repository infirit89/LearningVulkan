#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace LearningVulkan 
{
    enum class PresentMode 
    {
        Mailbox,
        Fifo,
        Imediate
    };

    class LogicalDevice;
    class Swapchain
    {
    public:
        Swapchain(LogicalDevice* logicalDevice, uint32_t width, uint32_t height, PresentMode presentMode);
        ~Swapchain();

        const VkExtent2D& GetExtent() const { return m_Extent; }
        const std::vector<VkImageView>& GetImageViews() const { return m_ImageViews; }
        const VkSurfaceFormatKHR& GetSurfaceFormat() const { return m_SurfaceFormat; }
        VkSwapchainKHR GetVulkanSwapchain() const { return m_Swapchain; }
        void Resize(uint32_t width, uint32_t height);
        void Present(VkSemaphore semaphore, uint32_t imageIndex);
        void AcquireNextImage(VkSemaphore imageAcquireSemaphore, uint32_t& imageIndex);

    private:
        void Create();
        void Destroy(VkSwapchainKHR swapchain);
        VkSurfaceFormatKHR ChooseCorrectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats);
        VkPresentModeKHR ChooseSurfacePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
        VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

    private:
        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
        uint32_t m_Width, m_Height;
        PresentMode m_DesiredPresentMode;
        LogicalDevice* m_LogicalDevice;
        VkSurfaceFormatKHR m_SurfaceFormat;
        std::vector<VkImage> m_Images;
        std::vector<VkImageView> m_ImageViews;
        VkExtent2D m_Extent;
    };
}
