#pragma once
#include <filesystem>
#include "interfaces/asset.h"
#include "interfaces/file_reader.h"

class Texture : public IAsset, IFileReader
{
protected:
	VkImage mImage;
	VkDeviceMemory mImageMemory;
	VkImageView mImageView;
	uint32_t mMipLevels;

public:
	Texture(const std::filesystem::path& _filePath, uint32_t _mipLevels = 0, VkImageCreateFlags _flags = 0, uint32_t _layers = 1, VkImageViewType _viewType = VK_IMAGE_VIEW_TYPE_2D);
	~Texture();

	inline const VkImageView& GetImageView() const { return mImageView; }

protected:
	void CreateImage(const std::array<std::filesystem::path, 6>& _filePaths, VkImageCreateFlags _flags, uint32_t _mipLevels, uint32_t _layers);
	void CopyIntoImage(const std::filesystem::path& _filePath, uint8_t _layer);
	void CopyIntoImage(int _width, int _height, stbi_uc* _pixels, uint8_t _layer);

	void GenerateMipMaps(VkFormat _imageFormat, int32_t _texWidth, int32_t _texHeight, uint32_t _layer) const;
};

class CubemapTexture : public Texture
{
	// For cubemaps use texPaths:
	// 0..5 -> right, left, top, bottom, front, back

public:
	CubemapTexture(const std::filesystem::path& _filePath)
		: Texture(_filePath, 1, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 6, VK_IMAGE_VIEW_TYPE_CUBE) {}
};

class RuntimeTexture
{
private:
	VkImage mImage;
	VkDeviceMemory mImageMemory;
	VkImageView mImageView;

public:
	inline const VkImageView& GetImageView() { return mImageView; }

	void CreateImage(VkExtent2D _extent, VkSampleCountFlagBits _numSamples, VkFormat _format, VkImageUsageFlags _usage, VkImageAspectFlags _aspectFlags);
	void CleanupImage() const;
};