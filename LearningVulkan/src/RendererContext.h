#pragma once

#include "PhysicalDevice.h"
#include "Swapchain.h"
#include "Framebuffer.h"

#include <vulkan/vulkan.h>

#include <string_view>
#include <vector>

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
		VkBuffer UniformBuffer;
		VkDeviceMemory UniformBufferMemory;
		void* UniformBufferMap;
	};

	class RendererContext 
	{
	public:
		RendererContext(std::string_view applicationName);
		~RendererContext();

		static VkInstance GetVulkanInstance() { return m_Instance; }
		static VkSurfaceKHR GetVulkanSurface() { return m_Surface; }

		const PhysicalDevice* GetPhysicalDevice() const { return m_PhysicalDevice; }
		static LogicalDevice* GetLogicalDevice() { return m_LogicalDevice; }
		Swapchain* GetSwapchain() const { return m_Swapchain; }
		VkRenderPass GetRenderPass() const { return m_RenderPass; }

		void Resize(uint32_t width, uint32_t height) const;
		const std::vector<Framebuffer*>& GetFramebuffers() const { return m_Framebuffers; }
		const PerFrameData& GetPerFrameData(size_t index) const { return m_PerFrameData.at(index); }
		size_t GetPerFrameDataSize() const { return m_PerFrameData.size(); }
		const VkPipeline& GetGraphicsPipeline() const { return m_Pipeline; }
		void DrawFrame();

	private:
		static void CreateVulkanInstance(std::string_view applicationName);
		static bool CheckLayersAvailability();
		void SetupDebugMessenger();
		static void CreateSurface();
		void CreateRenderPass();

		VkCommandPool CreateCommandPool(VkCommandPoolCreateFlags commandPoolFlags, uint32_t queueFamilyIndex) const;

		static VkCommandBuffer AllocateCommandBuffer(VkCommandPool commandPool);
		void RecordCommandBuffer(uint32_t imageIndex, VkCommandBuffer commandBuffer) const;
		static void CreateSyncObjects(VkSemaphore& swapchainImageAcquireSemaphore, VkSemaphore& queueReadySemaphore, VkFence& presentFence);

		void CreatePerFrameObjects(uint32_t frameIndex);
		void CreateGraphicsPipeline();
		static VkShaderModule CreateShader(const std::vector<char>& shaderData);
		void CreateBuffer(VkBufferUsageFlags usage, VkDeviceSize size, VkMemoryPropertyFlags memoryProperties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
		void CreateVertexBuffer();
		void CopyBuffer(VkBuffer sourceBuffer, VkBuffer destinationBuffer, VkDeviceSize size);
		void CreateIndexBuffer();
		void CreateCameraDescriptorSetLayout();
		void UpdateUniformBuffer(uint32_t frameIndex);
		void CreateDescriptorPool();
		void CreateDescriptorSets();
		void CreateTexture();
		void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling imageTiling, VkImageUsageFlags imageUsage, VkMemoryPropertyFlags memoryProperties, VkImage& image, VkDeviceMemory& imageMemory);
		void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
		void CopyBufferToImage(VkBuffer source, VkImage destination, uint32_t width, uint32_t height);
		void BeginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags usageFlags);
		void EndCommandBuffer(VkCommandBuffer commandBuffer);
		void CreateImageView();
		void CreateImageSampler();
		
	private:
		VkCommandPool m_TransientTransferCommandPool;
		VkCommandPool m_TransientGraphicsCommandPool;
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
		VkBuffer m_VertexBuffer;
		VkDeviceMemory m_VertexBufferMemory;

		// index buffer:
		VkBuffer m_IndexBuffer;
		VkDeviceMemory m_IndexBufferMemory;

		VkDescriptorSetLayout m_CameraDescriptorSetLayout;
		VkDescriptorPool m_DescriptorPool;
		std::vector<VkDescriptorSet> m_DescriptorSets;

		std::vector<Vertex> m_Vertices = {
			{ .Position = { 0.5, -0.5 }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {0.0f, 1.0f}, },
			{ .Position = { 0.5, 0.5 }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {0.0f, 0.0f}, },
			{ .Position = { -0.5, 0.5 }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {1.0f, 0.0f}, },
			{ .Position = { -0.5, -0.5 }, .Color = { 1.0, 1.0, 1.0 }, .TextureCoordinates = {1.0f, 1.0f}, },
		};

		std::vector<uint32_t> m_Indices = {
			0, 1, 2,
			2, 3, 0
		};

		VkImage m_TestImage;
		VkDeviceMemory m_TestImageMemory;
		VkImageView m_TestImageView;
		VkSampler m_TestImageSampler;
	};
}
