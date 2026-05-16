#pragma once

namespace Utils {
	namespace BufferUtils {
		uint32_t FindMemoryType(uint32_t _typeFilter, VkMemoryPropertyFlags _properties);
		void CreateBuffer(VkDeviceSize _size, VkBufferUsageFlags _usage, VkMemoryPropertyFlags _properties, VkBuffer& _buffer, VkDeviceMemory& _bufferMemory);
		void CopyBuffer(VkBuffer _srcBuffer, VkBuffer _dstBuffer, VkDeviceSize _size);
	}
}