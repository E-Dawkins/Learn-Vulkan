#pragma once
#include <filesystem>
#include "interfaces/asset.h"

class Texture : public IAsset
{
private:
	VkImage mImage;
	VkDeviceMemory mImageMemory;
	VkImageView mImageView;
	uint32_t mMipLevels;

public:
	Texture(const std::filesystem::path& _filePath);
	~Texture();

	const VkImageView& GetImageView() const { return mImageView; }

private:
	void CreateTextureImage(const std::filesystem::path& _filePath);
	void GenerateMipMaps(VkFormat _imageFormat, int32_t _texWidth, int32_t _texHeight) const;
};