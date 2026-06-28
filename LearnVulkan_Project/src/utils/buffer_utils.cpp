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

void Utils::BufferUtils::Ssbo::Init(VkDeviceSize _size) {
	mBufferSize = _size;

	Utils::BufferUtils::CreateBuffer(
		_size,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		mBuffer,
		mDeviceMemory
	);

	void* mapping = nullptr;
	vkMapMemory(App::GetInstance().GetLogicalDevice(), mDeviceMemory, 0, VK_WHOLE_SIZE, 0, &mapping);
	mPersistentMapping = mapping;
}

void Utils::BufferUtils::Ssbo::Reset() {
	const VkDevice& logicalDevice = App::GetInstance().GetLogicalDevice();

	vkUnmapMemory(logicalDevice, mDeviceMemory);
	mPersistentMapping = nullptr;

	vkDestroyBuffer(logicalDevice, mBuffer, nullptr);
	vkFreeMemory(logicalDevice, mDeviceMemory, nullptr);

}

void Utils::BufferUtils::Ssbo::WriteToDescriptorSet(const VkDescriptorSet& _set, uint32_t _binding) {
	VkDescriptorBufferInfo bufferInfo{
		.buffer = mBuffer,
		.offset = 0,
		.range = mBufferSize
	};

	VkWriteDescriptorSet descriptorWrite{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = _set,
		.dstBinding = _binding,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pBufferInfo = &bufferInfo
	};

	vkUpdateDescriptorSets(
		App::GetInstance().GetLogicalDevice(),

		// Write array
		1,
		&descriptorWrite,

		// Copy array
		0,
		nullptr
	);
}
