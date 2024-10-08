#pragma once
#include <vulkan/vulkan_core.h>

#include "GPUBuffer.h"
#include "Image.h"

namespace LearningVulkan
{
    enum class CommandBufferUsage
    {
        None = 0,
        OneTimeSubmit = 1,
    };

    class CommandBuffer
    {
    public:
        // NOTE: transfers ownership to this class
        CommandBuffer(const VkCommandPool& commandPool, VkCommandBuffer&& commandBuffer);

        //CommandBuffer(const VkCommandPool& commandPool, VkCommandBufferLevel commandBufferLevel);
        ~CommandBuffer();
        CommandBuffer(const CommandBuffer& other) = delete;
        CommandBuffer(CommandBuffer&& other) noexcept;

        CommandBuffer& operator=(CommandBuffer&& other) noexcept;
        CommandBuffer& operator=(CommandBuffer& other) = delete;

        void Begin(CommandBufferUsage commandBufferUsage = CommandBufferUsage::None);
        void End();
        void BeginRenderPass(const VkRenderPassBeginInfo& renderPass);
        void EndRenderPass();

        void BindPipeline(const VkPipeline& pipeline);
        // TODO: bind vertex buffers
        void BindVertexBuffer(const GPUBuffer* buffer);
        void BindIndexBuffer(const GPUBuffer* buffer);

        void SetScissor(const VkRect2D& scissorState);
        void SetViewport(const VkViewport& viewport);

        void BindDescriptorSets(const VkPipelineLayout& pipelineLayout, const VkDescriptorSet&);

        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

        const VkCommandBuffer& GetVulkanCommandBuffer() const;

        void TransitionLayout(Image* image, VkImageLayout newLayout);
        void CopyBufferToImage(GPUBuffer* source, Image* destination, uint32_t width, uint32_t height);
        void CopyBuffer(const GPUBuffer* source, const GPUBuffer* destination, size_t size);

    private:
        //void AllocateCommandBuffer(VkCommandPool commandPool, VkCommandBufferLevel commandBufferLevel);

    private:
        VkCommandBuffer m_CommandBuffer;
        VkCommandPool m_CommandPool;

        friend class RendererContext;
    };
}
