#include "renderer/mesh.h"

VkVertexInputBindingDescription Vertex::GetBindingDescription() {
	static VkVertexInputBindingDescription bindingDescription{
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};

	return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::GetAttributeDescriptions() {
	static std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{
		// Position attribute
		VkVertexInputAttributeDescription{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, pos)
		},

		// Color attribute
		VkVertexInputAttributeDescription{
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, color)
		},

		// Texcoord attribute
		VkVertexInputAttributeDescription{
			.location = 2,
			.binding = 0,
			.format = VK_FORMAT_R32G32_SFLOAT,
			.offset = offsetof(Vertex, texCoord)
		}
	};

	return attributeDescriptions;
}

bool Vertex::operator == (const Vertex& _other) const {
	return pos == _other.pos && color == _other.color && texCoord == _other.texCoord;
}
