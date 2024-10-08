#pragma once

#include "Image.h"
#include "PhysicalDevice.h"
#include "Swapchain.h"
#include "Framebuffer.h"
#include "GPUBuffer.h"

#include <string_view>
#include <vector>

#include "CommandBuffer.h"
#include "Sampler.h"
#include "Vertex.h"

namespace LearningVulkan 
{
    // TODO: move inside render context
    struct PerFrameData
    {
        CommandBuffer* CommandBuffer;
        //VkCommandBuffer CommandBuffer;
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

        static VkInstance GetVulkanInstance();
        static VkSurfaceKHR GetVulkanSurface();

        static VkCommandPool GetTransientTransferCommandPool();

        static VkCommandPool GetTransientGraphicsCommandPool();

        const PhysicalDevice* GetPhysicalDevice() const;

        static LogicalDevice* GetLogicalDevice();
        Swapchain* GetSwapchain() const;
        VkRenderPass GetRenderPass() const;

        void Resize(uint32_t width, uint32_t height);
        const std::vector<Framebuffer*>& GetFramebuffers() const;

        const PerFrameData& GetPerFrameData(size_t index) const;

        size_t GetPerFrameDataSize() const;

        const VkPipeline& GetGraphicsPipeline() const;
        void DrawFrame();

        static CommandBuffer CreateStackCommandBuffer(
            VkCommandPool commandPool);

        static CommandBuffer* CreateCommandBuffer(
            VkCommandPool commandPool);
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
            uint32_t imageIndex, CommandBuffer& commandBuffer);
        static void CreateSyncObjects(
            VkSemaphore& swapchainImageAcquireSemaphore,
            VkSemaphore& queueReadySemaphore,
            VkFence& presentFence);

        void CreatePerFrameObjects(uint32_t frameIndex);
        void CreateGraphicsPipeline();
        static VkShaderModule CreateShader(
            const std::vector<char>& shaderData);
        void CreateVertexBuffer();
        void CreateIndexBuffer();
        void CreateCameraDescriptorSetLayout();
        void UpdateUniformBuffer(uint32_t frameIndex);
        void CreateDescriptorPool();
        void CreateDescriptorSets();
        void CreateTexture();
        
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
    };
}

