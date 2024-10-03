#pragma once

#include "Image.h"
#include "PhysicalDevice.h"
#include "Swapchain.h"
#include "Framebuffer.h"
#include "GPUBuffer.h"

#include <vulkan/vulkan.h>

#include <string_view>
#include <vector>

#include "Sampler.h"
#include "Vertex.h"

namespace LearningVulkan 
{
    // TODO: move inside render context
    struct PerFrameData
    {
        VkCommandBuffer CommandBuffer;
        VkCommandPool CommandPool;
        VkFence PresentFence;
        VkSemaphore SwapchainImageAcquireSemaphore;
        VkSemaphore QueueReadySemaphore;

        // uniform buffer:
        GPUBuffer* CameraUniformBuffer;
        void* CameraUniformBufferMemory;
    };

    class RendererContext 
    {
    public:
        RendererContext(std::string_view applicationName);
        ~RendererContext();

        static VkInstance GetVulkanInstance() { return m_Instance; }
        static VkSurfaceKHR GetVulkanSurface() { return m_Surface; }
        static VkCommandPool GetTransientTransferCommandPool() 
        {
            return m_TransientTransferCommandPool;
        }
        static VkCommandPool GetTransientGraphicsCommandPool()
        {
            return m_TransientGraphicsCommandPool;
        }

        const PhysicalDevice* GetPhysicalDevice() const 
        { return m_PhysicalDevice; }
        static LogicalDevice* GetLogicalDevice() 
        { return m_LogicalDevice; }
        Swapchain* GetSwapchain() const { return m_Swapchain; }
        VkRenderPass GetRenderPass() const { return m_RenderPass; }

        void Resize(uint32_t width, uint32_t height);
        const std::vector<Framebuffer*>& GetFramebuffers() const 
        { return m_Framebuffers; }
        const PerFrameData& GetPerFrameData(size_t index) const 
        { return m_PerFrameData.at(index); }
        size_t GetPerFrameDataSize() const 
        { return m_PerFrameData.size(); }
        const VkPipeline& GetGraphicsPipeline() const 
        { return m_Pipeline; }
        void DrawFrame();

        static VkCommandBuffer AllocateCommandBuffer(
            VkCommandPool commandPool);
        static void BeginCommandBuffer(VkCommandBuffer commandBuffer,
              VkCommandBufferUsageFlags usageFlags);
        static void EndCommandBuffer(VkCommandBuffer commandBuffer);
    private:
        static void CreateVulkanInstance(std::string_view applicationName);
        static bool CheckLayersAvailability();
        void SetupDebugMessenger();
        static void CreateSurface();
        void CreateRenderPass();

        VkCommandPool CreateCommandPool(
            VkCommandPoolCreateFlags commandPoolFlags, 
            uint32_t queueFamilyIndex) const;

        void RecordCommandBuffer(
            uint32_t imageIndex, VkCommandBuffer commandBuffer);
        static void CreateSyncObjects(
            VkSemaphore& swapchainImageAcquireSemaphore,
            VkSemaphore& queueReadySemaphore,
            VkFence& presentFence);

        void CreatePerFrameObjects(uint32_t frameIndex);
        void CreateGraphicsPipeline();
        static VkShaderModule CreateShader(
            const std::vector<char>& shaderData);
        void CreateVertexBuffer();
        void CopyBuffer(
            VkBuffer sourceBuffer,
            VkBuffer destinationBuffer, VkDeviceSize size);
        void CreateIndexBuffer();
        void CreateCameraDescriptorSetLayout();
        void UpdateUniformBuffer(uint32_t frameIndex);
        void CreateDescriptorPool();
        void CreateDescriptorSets();
        void CreateTexture();
        void CreateImage(uint32_t width, uint32_t height,
            VkFormat format, VkImageTiling imageTiling,
            VkImageUsageFlags imageUsage,
            VkMemoryPropertyFlags memoryProperties,
            VkImage& image, VkDeviceMemory& imageMemory);
        void TransitionImageLayout(VkImage image,
            VkImageLayout oldLayout, VkImageLayout newLayout);
        void CopyBufferToImage(VkBuffer source, VkImage destination,
             uint32_t width, uint32_t height);
        void CreateImageView(const VkImage& image,
               VkFormat image_format,
               VkImageAspectFlags image_aspect_flags,
               VkImageView& image_view);
        void CreateDepthResources();

        void AddCube(
            const glm::mat4& transformMatrix = glm::mat4(1.0f));

    private:
        static VkCommandPool m_TransientTransferCommandPool;
        static VkCommandPool m_TransientGraphicsCommandPool;
        static VkInstance m_Instance;
        VkDebugUtilsMessengerEXT m_DebugMessenger;
        static VkSurfaceKHR m_Surface;
        static LogicalDevice* m_LogicalDevice;
        VkRenderPass m_RenderPass;
        PhysicalDevice* m_PhysicalDevice;
        Swapchain* m_Swapchain;
        std::vector<Framebuffer*> m_Framebuffers;

        VkPipelineLayout m_PipelineLayout;
        VkPipeline m_Pipeline;
        uint32_t m_FrameIndex = 0;

        std::vector<PerFrameData> m_PerFrameData;

        // vertex buffer:
        GPUBuffer* m_VertexBuffer;

        // index buffer:
        GPUBuffer* m_IndexBuffer;
        
        VkDescriptorSetLayout m_CameraDescriptorSetLayout;
        VkDescriptorPool m_DescriptorPool;
        std::vector<VkDescriptorSet> m_DescriptorSets;

        std::vector<Vertex> m_Vertices;
        std::vector<uint32_t> m_Indices;
        uint32_t currentIndex = 0;

        Image* m_TestImage;
        Sampler* m_TestImageSampler;

        Image* m_DepthImage;
    };
}

