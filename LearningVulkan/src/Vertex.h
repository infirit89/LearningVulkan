#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <array>

namespace LearningVulkan
{
	struct Vertex
	{
		glm::vec2 Position;
		glm::vec3 Color;

		static VkVertexInputBindingDescription GetBindingDescription()
		{
			return {
				.binding = 0,
				.stride = sizeof(Vertex),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
			};
		}

		static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions()
		{
			std::array attributeDescriptions = {
				VkVertexInputAttributeDescription { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, Position) },
				VkVertexInputAttributeDescription { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, Color) },
			};

			return attributeDescriptions;
		}
	};

	struct CameraData
	{
		glm::mat4 Projection;
		glm::mat4 View;
		glm::mat4 Model;
	};
}
