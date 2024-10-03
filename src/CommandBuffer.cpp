#include "CommandBuffer.h"

#include "LogicalDevice.h"
#include "RendererContext.h"

namespace LearningVulkan 
{
    CommandBuffer::CommandBuffer(const VkCommandPool& commandPool, 
        VkCommandBuffer&& commandBuffer)
        : m_CommandBuffer(commandBuffer), m_CommandPool(commandPool)
    {
    }

    /*CommandBuffer::CommandBuffer(const VkCommandPool& commandPool,
        VkCommandBufferLevel commandBufferLevel)
        : m_CommandPool(commandPool)
    {
        AllocateCommandBuffer(commandPool, commandBufferLevel);
    }*/

    CommandBuffer::~CommandBuffer()
    {
        LogicalDevice* logicalDevice = RendererContext::GetLogicalDevice();
        vkFreeCommandBuffers(logicalDevice->GetVulkanDevice(), m_CommandPool, 
            1, &m_CommandBuffer);
    }

    void CommandBuffer::Begin()
    {
        VkCommandBufferBeginInfo commandBufferBeginInfo{};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        assert(vkBeginCommandBuffer(m_CommandBuffer, &commandBufferBeginInfo) == VK_SUCCESS);
    }

    void CommandBuffer::End()
    {
        assert(vkEndCommandBuffer(m_CommandBuffer) == VK_SUCCESS);
    }

    void CommandBuffer::BeginRenderPass(const VkRenderPassBeginInfo& renderPass)
    {
        vkCmdBeginRenderPass(m_CommandBuffer, &renderPass, VK_SUBPASS_CONTENTS_INLINE);
    }

    void CommandBuffer::EndRenderPass()
    {
        vkCmdEndRenderPass(m_CommandBuffer);
    }

    void CommandBuffer::BindPipeline(const VkPipeline& pipeline)
    {
        vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }

    void CommandBuffer::BindVertexBuffer(const GPUBuffer* buffer)
    {
        VkBuffer vertexBuffer = buffer->GetVulkanBuffer();
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, &vertexBuffer, offsets);
    }

    void CommandBuffer::BindIndexBuffer(const GPUBuffer* buffer)
    {
        vkCmdBindIndexBuffer(m_CommandBuffer, buffer->GetVulkanBuffer(), 0, VK_INDEX_TYPE_UINT32);
    }

    void CommandBuffer::SetScissor(const VkRect2D& scissorState)
    {
        vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissorState);
    }

    void CommandBuffer::SetViewport(const VkViewport& viewport)
    {
        vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);
    }

    void CommandBuffer::BindDescriptorSets(const VkPipelineLayout& pipelineLayout, const VkDescriptorSet& descriptorSet)
    {
        vkCmdBindDescriptorSets(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
            pipelineLayout, 0, 1, &descriptorSet, 
            0, nullptr);
    }

    void CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
        int32_t vertexOffset, uint32_t firstInstance)
    {
        vkCmdDrawIndexed(m_CommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    /*void CommandBuffer::AllocateCommandBuffer(VkCommandPool commandPool, 
        VkCommandBufferLevel commandBufferLevel)
    {
        VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
        commandBufferAllocateInfo.commandPool = commandPool;
        commandBufferAllocateInfo.
    }*/
}
