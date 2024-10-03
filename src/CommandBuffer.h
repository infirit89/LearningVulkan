#pragma once
#include <vulkan/vulkan_core.h>

#include "GPUBuffer.h"

namespace LearningVulkan
{
    class CommandBuffer
    {
    public:
        // NOTE: transfers ownership to this class
        CommandBuffer(const VkCommandPool& commandPool, VkCommandBuffer&& commandBuffer);

        //CommandBuffer(const VkCommandPool& commandPool, VkCommandBufferLevel commandBufferLevel);
        ~CommandBuffer();

        void Begin();
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

        const VkCommandBuffer& GetVulkanCommandBuffer() const
        {
            return m_CommandBuffer;
        }

    private:
        //void AllocateCommandBuffer(VkCommandPool commandPool, VkCommandBufferLevel commandBufferLevel);

    private:
        VkCommandBuffer m_CommandBuffer;
        VkCommandPool m_CommandPool;
    };
}
