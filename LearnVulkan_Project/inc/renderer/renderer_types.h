#pragma once
#include "utils/buffer_utils.h"
#include "utils/type_defs.h"

class Texture;
class CubemapTexture;
class Material;

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool IsComplete() const {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct MultisampleState
{
	VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
	VkBool32 sampleShadingEnabled = VK_FALSE;
	float minSampleShading = 1.f;
};

struct UniformBuffer
{
	VkBuffer buffer;
	VkDeviceMemory memory;
	void* mapped; // points to GPU memory
};

class FrameData
{
private:
	UniformBuffer mUniformBuffer;

	VkDescriptorPool mDescriptorPool;
	VkDescriptorSet mDescriptorSet;

	VkCommandBuffer mCommandBuffer;

	VkSemaphore mImageAvailableSemaphore; // signal that an image has been acquired
	VkFence mInFlightFence; // make sure only one frame is rendering at a time

public:
	void Init(VkDevice _logicalDevice, VkCommandPool _commandPool);
	~FrameData();

	inline const UniformBuffer& GetUniformBuffer() const { return mUniformBuffer; }
	inline const VkDescriptorSet& GetDescriptorSet() const { return mDescriptorSet; }
	inline const VkCommandBuffer& GetCommandBuffer() const { return mCommandBuffer; }
	inline const VkSemaphore& GetImageAvailableSemaphore() const { return mImageAvailableSemaphore; }
	inline const VkFence& GetInFlightFence() const { return mInFlightFence; }

private:
	void CreateUniformBuffer(VkDevice _logicalDevice);
	void CreateDescriptorPool(VkDevice _logicalDevice);
	void CreateDescriptorSet(VkDevice _logicalDevice);
	void CreateCommandBuffer(VkDevice _logicalDevice, VkCommandPool _commandPool);
	void CreateSyncObjects(VkDevice _logicalDevice);
};

class MaterialData
{
private:
	VkDescriptorSet mDescriptorSet;

	std::unordered_map<AssetDefs::TextureSlot, VkSampler> mTextureSlotToSampler;
	Utils::BufferUtils::Ssbo mDenseIdToTextureSlot;

	std::unordered_map<AssetDefs::CubemapSlot, VkSampler> mCubemapSlotToSampler;
	Utils::BufferUtils::Ssbo mDenseIdToCubemapSlot;

	Utils::BufferUtils::Ssbo mMaterialParamsBuffer;
	Utils::BufferUtils::Ssbo mDenseIdToMaterialSlot;

public:
	void Init(VkDevice _logicalDevice, VkDescriptorPool _descriptorPool);
	void Reset();

	void OnTextureLoaded(std::weak_ptr<Texture> _tex, AssetDefs::DenseId _denseId, AssetDefs::TextureSlot _texSlot);
	void OnTextureUnloaded(AssetDefs::DenseId _denseId);
	void OnCubemapLoaded(std::weak_ptr<CubemapTexture> _tex, AssetDefs::DenseId _denseId, AssetDefs::CubemapSlot _texSlot);
	void OnCubemapUnloaded(AssetDefs::DenseId _denseId);
	void OnMaterialLoaded(std::weak_ptr<Material> _mat, AssetDefs::DenseId _denseId, AssetDefs::MaterialSlot _matSlot);
	void OnMaterialUnloaded(AssetDefs::DenseId _denseId);

	inline const VkDescriptorSet& GetDescriptorSet() const { return mDescriptorSet; }

private:
	void InitBuffers();
	void CreateDescriptorSet(VkDevice _logicalDevice, VkDescriptorPool _descriptorPool);
	void CreateTextureSamplerForSlot(AssetDefs::TextureSlot _texSlot);
	void CreateCubemapSamplerForSlot(AssetDefs::CubemapSlot _texSlot);
};