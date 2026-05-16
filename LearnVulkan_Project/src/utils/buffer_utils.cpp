#include "pch.h"
#include "utils/buffer_utils.h"

#include "app.h"

uint32_t Utils::BufferUtils::FindMemoryType(uint32_t _typeFilter, VkMemoryPropertyFlags _properties) {
	VkPhysicalDeviceMemoryProperties memProperties = {};
	vkGetPhysicalDeviceMemoryProperties(App::GetInstance().GetPhysicalDevice(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		bool suitableMemoryType = (_typeFilter & (1 << i));
		bool allPropertiesSupported = (memProperties.memoryTypes[i].propertyFlags & _properties);

		if (suitableMemoryType && allPropertiesSupported) {
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type!");
}

void Utils::BufferUtils::CreateBuffer(VkDeviceSize _size, VkBufferUsageFlags _usage, VkMemoryPropertyFlags _properties, VkBuffer& _buffer, VkDeviceMemory& _bufferMemory) {
	VkBufferCreateInfo bufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = _size,
		.usage = _usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VkDevice& logicalDevice = App::GetInstance().GetLogicalDevice();
	if (vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &_buffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create buffer!");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetBufferMemoryRequirements(logicalDevice, _buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, _properties)
	};

	if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &_bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate buffer memory!");
	}

	vkBindBufferMemory(logicalDevice, _buffer, _bufferMemory, 0);
}

void Utils::BufferUtils::CopyBuffer(VkBuffer _srcBuffer, VkBuffer _dstBuffer, VkDeviceSize _size) {
	const App& appInst = App::GetInstance();
	VkCommandBuffer commandBuffer = appInst.BeginSingleTimeCommands();

	// Record copy command
	{
		VkBufferCopy copyRegion = {
			.srcOffset = 0, // optional
			.dstOffset = 0, // optional
			.size = _size,
		};

		vkCmdCopyBuffer(commandBuffer, _srcBuffer, _dstBuffer, 1, &copyRegion);
	}

	appInst.EndSingleTimeCommands(commandBuffer);
}
