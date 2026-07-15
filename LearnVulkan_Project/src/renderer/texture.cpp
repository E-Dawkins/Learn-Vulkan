#include "pch.h"
#include "renderer/texture.h"

#include "app.h"
#include "utils/buffer_utils.h"
#include "utils/image_utils.h"

Texture::Texture(const std::filesystem::path& _filePath, VkImageCreateFlags _flags, uint32_t _layers, VkImageViewType _viewType)
	: IAsset(_filePath)
{
	EnforceFileExtension(_filePath, ".texture");

	AilReader reader;
	ReadFileAsAil(_filePath, reader);

	// Max number of 'texPaths' is 6 because of cubemaps. Most
	// of the time it is 1, but this makes it more generic
	std::array<std::filesystem::path, 6> texPaths;
	if (auto texPathsNode = reader.TryGetNode("texPaths")) {
		for (size_t i = 0; i < 6; i++) {
			if (auto texPathINode = texPathsNode->TryGetSubnode(i)) {
				texPaths[i] = texPathINode->GetAsStr();
			}
		}
	}

	// mMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

	CreateImage(texPaths, _flags, 1, _layers);

	mImageView = Utils::ImageUtils::CreateImageView(mImage, _viewType, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, mMipLevels, _layers);
}

Texture::~Texture() {
	const VkDevice& logicalDevice = App::GetInstance().GetLogicalDevice();

	vkDestroyImageView(logicalDevice, mImageView, nullptr);
	vkDestroyImage(logicalDevice, mImage, nullptr);
	vkFreeMemory(logicalDevice, mImageMemory, nullptr);
}

void Texture::CreateImage(const std::array<std::filesystem::path, 6>& _filePaths, VkImageCreateFlags _flags, uint32_t _mipLevels, uint32_t _layers) {
	int texWidth = 0, texHeight = 0, texChannels = 0;
	stbi_uc* pixels = stbi_load(_filePaths[0].string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		throw std::runtime_error("Failed to load image for layer 0!");
	}

	Utils::ImageUtils::CreateImage(
		texWidth,
		texHeight,

		_flags,
		_mipLevels,
		_layers,

		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_SRGB,

		// Image object is optimal for shader access
		VK_IMAGE_TILING_OPTIMAL,

		// Source / destination for a buffer copy, and sampled in the shader
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,

		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mImage,
		mImageMemory
	);

	// Copy the staging buffer into the texture image
	Utils::ImageUtils::TransitionImageLayout(
		mImage,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		_mipLevels,
		_layers
	);

	mMipLevels = _mipLevels;

	for (uint8_t i = 0; i < _layers; i++) {
		if (i == 0) {
			// Direct copy first image to avoid loading it twice
			CopyIntoImage(texWidth, texHeight, pixels, 0);
		}
		else if (!_filePaths[i].empty()) {
			// Then copy others normally
			CopyIntoImage(_filePaths[i], i);
		}
	}
}

void Texture::CopyIntoImage(const std::filesystem::path& _filePath, uint8_t _layer) {
	int texWidth = 0, texHeight = 0, texChannels = 0;
	stbi_uc* pixels = stbi_load(_filePath.string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		throw std::runtime_error(std::format("Failed to load image for layer {}!", _layer));
	}

	CopyIntoImage(texWidth, texHeight, pixels, _layer);
}

void Texture::CopyIntoImage(int _width, int _height, stbi_uc* _pixels, uint8_t _layer) {
	const VkDevice& logicalDevice = App::GetInstance().GetLogicalDevice();

	// 4 bytes per pixel
	VkDeviceSize imageSize = static_cast<VkDeviceSize>(_width * _height * 4);

	// Create staging buffer
	VkBuffer stagingBuffer = {};
	VkDeviceMemory stagingBufferMemory = {};
	Utils::BufferUtils::CreateBuffer(
		imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	// Copy pixel data into device local memory
	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, _pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	// Free pixel data
	stbi_image_free(_pixels);

	Utils::ImageUtils::CopyBufferToImage(
		stagingBuffer,
		mImage,
		static_cast<uint32_t>(_width),
		static_cast<uint32_t>(_height),
		_layer
	);

	// Transition image for shader access is handled
	// for each mip level when generating them
	GenerateMipMaps(VK_FORMAT_R8G8B8A8_SRGB, _width, _height, _layer);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
}

void Texture::GenerateMipMaps(VkFormat _imageFormat, int32_t _texWidth, int32_t _texHeight, uint32_t _layer) const {
	// Check if image format supports linear blitting
	VkFormatProperties formatProperties = {};
	vkGetPhysicalDeviceFormatProperties(App::GetInstance().GetPhysicalDevice(), _imageFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("Texture image format does not support linear blitting!");
	}

	VkCommandBuffer commandBuffer = App::GetInstance().BeginSingleTimeCommands();

	VkImageMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = mImage,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.baseArrayLayer = _layer,
			.layerCount = 1,
		}
	};

	int32_t mipWidth = _texWidth, mipHeight = _texHeight;

	for (uint32_t i = 1; i < mMipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr, // memory barriers
			0, nullptr, // buffer memory barriers
			1, &barrier // image memory barriers
		);

		// Specify the regions that will be copied from / to
		VkImageBlit blit = {
			.srcSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = i - 1,
				.baseArrayLayer = _layer,
				.layerCount = 1
			},
			.srcOffsets = {
				{ 0, 0, 0 },
				{ mipWidth, mipHeight, 1 }
			},
			.dstSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = i,
				.baseArrayLayer = _layer,
				.layerCount = 1
			},
			.dstOffsets = {
				{ 0, 0, 0 },
				{ (mipWidth > 1 ? mipWidth / 2 : 1), (mipHeight > 1 ? mipHeight / 2 : 1), 1 }
			},
		};

		vkCmdBlitImage(commandBuffer,
			mImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR
		);

		// Transition mip level to shader read only
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr, // memory barriers
			0, nullptr, // buffer memory barriers
			1, &barrier // image memory barriers
		);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	// Transition mip level to shader read only
	barrier.subresourceRange.baseMipLevel = mMipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr, // memory barriers
		0, nullptr, // buffer memory barriers
		1, &barrier // image memory barriers
	);

	App::GetInstance().EndSingleTimeCommands(commandBuffer);
}

void RuntimeTexture::CreateImage(VkExtent2D _extent, VkSampleCountFlagBits _numSamples, VkFormat _format, VkImageUsageFlags _usage, VkImageAspectFlags _aspectFlags) {
	// For now, all runtime textures have no mip levels
	constexpr uint32_t mipLevels = 1;

	Utils::ImageUtils::CreateImage(
		_extent.width,
		_extent.height,
		0,
		mipLevels,
		1,
		_numSamples,
		_format,
		VK_IMAGE_TILING_OPTIMAL,
		_usage,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mImage,
		mImageMemory
	);

	mImageView = Utils::ImageUtils::CreateImageView(mImage, VK_IMAGE_VIEW_TYPE_2D, _format, _aspectFlags, mipLevels, 1);
}

void RuntimeTexture::CleanupImage() const {
	const VkDevice& logicalDevice = App::GetInstance().GetLogicalDevice();

	vkDestroyImageView(logicalDevice, mImageView, nullptr);
	vkDestroyImage(logicalDevice, mImage, nullptr);
	vkFreeMemory(logicalDevice, mImageMemory, nullptr);
}
