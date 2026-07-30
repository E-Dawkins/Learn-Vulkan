#pragma once
#include "utils/buffer_utils.h"
#include "utils/type_defs.h"

#include <stack>

class Texture;
class CubemapTexture;
class Material;
class MeshInstance;

struct RenderTransform;

// TODO: maybe move these enums into this file, rather than forwarding them?
enum class BlendModel : uint8_t;
enum class ShadingModel : uint8_t;

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

class MeshData
{
private:
	VkDescriptorSet mDescriptorSet;

	Utils::BufferUtils::Ssbo mInstanceTransforms;
	Utils::BufferUtils::Ssbo mInstToTransformIndex;
	std::stack<uint32_t> mFreeTransforms;

public:
	void Init(VkDevice _logicalDevice, VkDescriptorPool _descriptorPool);
	void Reset();

	void MapFreeTransform(RenderTransform& _value, uint32_t& _outIndex);
	void BindTransformIndices(const std::vector<uint32_t>& _transformIndices);

	inline const VkDescriptorSet& GetDescriptorSet() const { return mDescriptorSet; }

private:
	void InitBuffers();
	void CreateDescriptorSet(VkDevice _logicalDevice, VkDescriptorPool _descriptorPool);

};

class RenderBucketMap
{
public:
	using RenderPassHash = uint16_t;
	using RuntimeRenderId = uint16_t;
	using RenderBucket = std::unordered_map<RuntimeRenderId, std::shared_ptr<MeshInstance>>;

private:
	std::stack<RuntimeRenderId> mFreeIds;
	std::unordered_map<RenderPassHash, RenderBucket> mBuckets;

public:
	RenderBucketMap();

	RuntimeRenderId RegisterMeshInstance(std::shared_ptr<MeshInstance> _toRegister);
	void UnregisterMeshInstance(std::weak_ptr<MeshInstance> _toUnregister);

	inline const RenderBucket& GetBucket(RenderPassHash _hash) const { return mBuckets.at(_hash); }

	static constexpr RenderPassHash GetRenderPassHash(BlendModel _blend, ShadingModel _shading) {
		constexpr int halfBitCount = std::numeric_limits<RenderPassHash>::digits / 2;

		RenderPassHash shadingHash = static_cast<RenderPassHash>(_shading) << halfBitCount;
		RenderPassHash blendHash = static_cast<RenderPassHash>(_blend);

		return blendHash | shadingHash;
	}
};
