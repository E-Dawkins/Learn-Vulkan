#pragma once

namespace Utils {
	namespace ImageUtils {
		void CreateImage(uint32_t _width, uint32_t _height, VkImageCreateFlags _flags, uint32_t _mipLevels, uint32_t _arrayLayers, VkSampleCountFlagBits _numSamples, VkFormat _format, VkImageTiling _tiling, VkImageUsageFlags _usage, VkMemoryPropertyFlags _properties, VkImage& _image, VkDeviceMemory& _imageMemory);
		void TransitionImageLayout(VkImage _image, VkFormat _format, VkImageLayout _oldLayout, VkImageLayout _newLayout, uint32_t _mipLevels, uint32_t _layerCount);
		void CopyBufferToImage(VkBuffer _buffer, VkImage _image, uint32_t _width, uint32_t _height, uint32_t _baseLayer);
		VkImageView CreateImageView(VkImage _image, VkImageViewType _viewType, VkFormat _format, VkImageAspectFlags _aspectFlags, uint32_t _mipLevels, uint32_t _layerCount);
	}
}