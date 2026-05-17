#include "pch.h"
#include "utils/image_utils.h"

#include "app.h"
#include "utils/buffer_utils.h"

void Utils::ImageUtils::CreateImage(uint32_t _width, uint32_t _height, uint32_t _mipLevels, VkSampleCountFlagBits _numSamples, VkFormat _format, VkImageTiling _tiling, VkImageUsageFlags _usage, VkMemoryPropertyFlags _properties, VkImage& _image, VkDeviceMemory& _imageMemory) {
	const VkDevice& logicalDevice = App::GetInstance().GetLogicalDevice();

	VkImageCreateInfo imageInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = _format,
		.extent = {
			.width = _width,
			.height = _height,
			.depth = 1
		},
		.mipLevels = _mipLevels,
		.arrayLayers = 1,
		.samples = _numSamples,
		.tiling = _tiling,
		.usage = _usage,

		// Image only used by one queue family
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,

		// We do not care about the initial layout, as we are copying to it
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	if (vkCreateImage(logicalDevice, &imageInfo, nullptr, &_image) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create image!");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetImageMemoryRequirements(logicalDevice, _image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = Utils::BufferUtils::FindMemoryType(memRequirements.memoryTypeBits, _properties)
	};

	if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &_imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate image memory!");
	}

	vkBindImageMemory(logicalDevice, _image, _imageMemory, 0);
}

void Utils::ImageUtils::TransitionImageLayout(VkImage _image, VkFormat /*_format*/, VkImageLayout _oldLayout, VkImageLayout _newLayout, uint32_t _mipLevels) {
	VkCommandBuffer commandBuffer = App::GetInstance().BeginSingleTimeCommands();

	VkImageMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.oldLayout = _oldLayout,
		.newLayout = _newLayout,

		// Indicate we are not transferring queue family ownership,
		// this is not the default behaviour!
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

		.image = _image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = _mipLevels,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
	};

	VkPipelineStageFlags srcStage = {}, dstStage = {};

	// Undefined => transfer destination
	if (_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && _newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	// Transfer destination => shader reading
	else if (_oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && _newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	// Unknown
	else {
		throw std::invalid_argument("Unsupported layout transition!");
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		srcStage, dstStage,
		0,
		0, nullptr, // memory barriers
		0, nullptr, // buffer memory barries
		1, &barrier // image memory barriers
	);

	App::GetInstance().EndSingleTimeCommands(commandBuffer);
}

void Utils::ImageUtils::CopyBufferToImage(VkBuffer _buffer, VkImage _image, uint32_t _width, uint32_t _height) {
	VkCommandBuffer commandBuffer = App::GetInstance().BeginSingleTimeCommands();

	// Specify the region of the buffer copied to which part of the image
	VkBufferImageCopy region = {
		// Byte offset in the buffer that pixel data starts
		.bufferOffset = 0,

		// How are pixels laid out in memory?
		.bufferRowLength = 0,
		.bufferImageHeight = 0,

		// Which part of the image are we copying the pixels into?
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
		.imageOffset = { 0, 0, 0 },
		.imageExtent = { _width, _height, 1 }
	};

	vkCmdCopyBufferToImage(
		commandBuffer,
		_buffer,
		_image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	App::GetInstance().EndSingleTimeCommands(commandBuffer);
}

VkImageView Utils::ImageUtils::CreateImageView(VkImage _image, VkFormat _format, VkImageAspectFlags _aspectFlags, uint32_t _mipLevels) {
	VkImageViewCreateInfo viewInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = _image,

		// These specify how the image data is interpreted
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = _format,

		// This describes what the image purpose is, and which
		// part of the image should be accessed. For now our image
		// contains no mip levels or multiple layers
		.subresourceRange = {
			.aspectMask = _aspectFlags,
			.baseMipLevel = 0,
			.levelCount = _mipLevels,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	VkImageView imageView = {};
	if (vkCreateImageView(App::GetInstance().GetLogicalDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create image view!");
	}

	return imageView;
}