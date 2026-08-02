#include "pch.h"
#include "renderer/renderer_types.h"

#include "app.h"
#include "renderer/asset_manager.h"
#include "renderer/camera.h"
#include "renderer/material.h"
#include "renderer/pipeline_layout_manager.h"
#include "renderer/shader.h"
#include "utils/debug_logger.h"

void FrameData::Init(VkDevice _logicalDevice, VkCommandPool _commandPool) {
	LOG_MSG("Start init", LogVerbosity::Info);

	CreateUniformBuffer(_logicalDevice);
	CreateDescriptorPool(_logicalDevice);
	CreateDescriptorSet(_logicalDevice);
	CreateCommandBuffer(_logicalDevice, _commandPool);
	CreateSyncObjects(_logicalDevice);

	LOG_MSG("Finish init", LogVerbosity::Info);
}

FrameData::~FrameData() {
	const VkDevice& logicalDevice = App::GetInstance().GetLogicalDevice();

	vkDestroyBuffer(logicalDevice, mUniformBuffer.buffer, nullptr);
	vkFreeMemory(logicalDevice, mUniformBuffer.memory, nullptr);

	vkDestroyDescriptorPool(logicalDevice, mDescriptorPool, nullptr);

	vkDestroySemaphore(logicalDevice, mImageAvailableSemaphore, nullptr);
	vkDestroyFence(logicalDevice, mInFlightFence, nullptr);
}

void FrameData::CreateUniformBuffer(VkDevice _logicalDevice) {
	const VkDeviceSize bufferSize = sizeof(CameraData);

	Utils::BufferUtils::CreateBuffer(
		bufferSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		mUniformBuffer.buffer,
		mUniformBuffer.memory
	);

	vkMapMemory(_logicalDevice, mUniformBuffer.memory, 0, bufferSize, 0, &mUniformBuffer.mapped);
}

void FrameData::CreateDescriptorPool(VkDevice _logicalDevice) {
	// Our descriptor set will contain a single uniform buffer for now
	VkDescriptorPoolSize framePoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };

	// We have already said how many descriptors are available,
	// now tell Vulkan to allocate a single set via '.maxSets'
	VkDescriptorPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = 1,
		.pPoolSizes = &framePoolSize,
	};

	if (vkCreateDescriptorPool(_logicalDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create frame descriptor pool!");
	}
}

void FrameData::CreateDescriptorSet(VkDevice _logicalDevice) {
	const VkDescriptorSetLayout& layoutPreset = PipelineLayoutManager::GetInstance().GetDescriptorSetLayout(DescriptorSet::Global);

	VkDescriptorSetAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = mDescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layoutPreset
	};

	if (vkAllocateDescriptorSets(_logicalDevice, &allocInfo, &mDescriptorSet) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate frame descriptor set!");
	}

	// Which region within our buffer contains the descriptor data?
	VkDescriptorBufferInfo bufferInfo{
		.buffer = mUniformBuffer.buffer,
		.offset = 0,
		.range = sizeof(CameraData)
	};

	VkWriteDescriptorSet descriptorWrite{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = mDescriptorSet,
		.dstBinding = 0,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &bufferInfo
	};

	// Apply updates - this accepts write arrays, and copy arrays
	vkUpdateDescriptorSets(
		_logicalDevice,

		// Write array
		1,
		&descriptorWrite,

		// Copy array
		0,
		nullptr
	);
}

void FrameData::CreateCommandBuffer(VkDevice _logicalDevice, VkCommandPool _commandPool) {
	VkCommandBufferAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = _commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	if (vkAllocateCommandBuffers(_logicalDevice, &allocInfo, &mCommandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate command buffer for frame!");
	}
}

void FrameData::CreateSyncObjects(VkDevice _logicalDevice) {
	VkSemaphoreCreateInfo semaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	// Create the fence in the signalled state, so that on the
	// first frame we do not wait infinitely
	VkFenceCreateInfo fenceInfo = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};

	if (vkCreateSemaphore(_logicalDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &mInFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create sync objects for a frame!");
	}
}

void MaterialData::Init(VkDevice _logicalDevice, VkDescriptorPool _descriptorPool) {
	InitBuffers();
	CreateDescriptorSet(_logicalDevice, _descriptorPool);
}

void MaterialData::Reset() {
	mDenseIdToTextureSlot.Reset();
	mDenseIdToCubemapSlot.Reset();
	mMaterialParamsBuffer.Reset();
	mDenseIdToMaterialSlot.Reset();
}

void MaterialData::OnTextureLoaded(std::weak_ptr<Texture> _tex, AssetDefs::DenseId _denseId, AssetDefs::TextureSlot _texSlot) {
	auto tex = _tex.lock();
	if (!tex) {
		std::cout << "OnTextureLoaded: '_tex' is invalid!\n";
		return;
	}

	mDenseIdToTextureSlot.GetElement<AssetDefs::TextureSlot>(_denseId) = _texSlot;

	// We need a new texture sampler for this texture index
	if (!mTextureSlotToSampler.contains(_texSlot)) {
		CreateTextureSamplerForSlot(_texSlot);
	}

	VkDescriptorImageInfo imageInfo{
		.sampler = mTextureSlotToSampler[_texSlot],
		.imageView = tex->GetImageView(),
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkWriteDescriptorSet descriptorWrite{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = mDescriptorSet,
		.dstBinding = 0,
		.dstArrayElement = _texSlot,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &imageInfo
	};

	// Apply updates - this accepts write arrays, and copy arrays
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

void MaterialData::OnTextureUnloaded(AssetDefs::DenseId _denseId) {
	AssetDefs::TextureSlot& textureSlot = mDenseIdToTextureSlot.GetElement<AssetDefs::TextureSlot>(_denseId);

	// Destroy the sampler at texture slot
	vkDestroySampler(App::GetInstance().GetLogicalDevice(), mTextureSlotToSampler[textureSlot], nullptr);

	// Remove slot -> sampler mapping
	mTextureSlotToSampler.erase(textureSlot);

	// Point slot to 'default_tex', in the case that a material is still referencing it
	textureSlot = 0;
}

void MaterialData::OnCubemapLoaded(std::weak_ptr<CubemapTexture> _tex, AssetDefs::DenseId _denseId, AssetDefs::CubemapSlot _texSlot) {
	auto tex = _tex.lock();
	if (!tex) {
		std::cout << "OnCubemapLoaded: '_tex' is invalid!\n";
		return;
	}

	mDenseIdToCubemapSlot.GetElement<AssetDefs::CubemapSlot>(_denseId) = _texSlot;

	// We need a new texture sampler for this texture index
	if (!mCubemapSlotToSampler.contains(_texSlot)) {
		CreateCubemapSamplerForSlot(_texSlot);
	}

	VkDescriptorImageInfo imageInfo{
		.sampler = mCubemapSlotToSampler[_texSlot],
		.imageView = tex->GetImageView(),
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkWriteDescriptorSet descriptorWrite{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = mDescriptorSet,
		.dstBinding = 2,
		.dstArrayElement = _texSlot,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &imageInfo
	};

	// Apply updates - this accepts write arrays, and copy arrays
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

void MaterialData::OnCubemapUnloaded(AssetDefs::DenseId _denseId) {
	AssetDefs::CubemapSlot& cubemapSlot = mDenseIdToCubemapSlot.GetElement<AssetDefs::CubemapSlot>(_denseId);

	// Destroy the sampler at texture slot
	vkDestroySampler(App::GetInstance().GetLogicalDevice(), mCubemapSlotToSampler[cubemapSlot], nullptr);

	// Remove slot -> sampler mapping
	mCubemapSlotToSampler.erase(cubemapSlot);

	// Point slot to 'default_tex', in the case that a material is still referencing it
	cubemapSlot = 0;
}

void MaterialData::OnMaterialLoaded(std::weak_ptr<Material> _mat, AssetDefs::DenseId _denseId, AssetDefs::MaterialSlot _matSlot) {
	auto mat = _mat.lock();
	if (!mat) {
		std::cout << "OnMaterialLoaded: '_mat' is invalid!\n";
		return;
	}

	MaterialParams& paramsToFill = mMaterialParamsBuffer.GetElement<MaterialParams>(_denseId);
	mDenseIdToMaterialSlot.GetElement<AssetDefs::MaterialSlot>(_denseId) = _matSlot;

	// Copy material params into ssbo
	paramsToFill = *mat->params;

	// Free CPU-side pointer
	delete mat->params;

	// Set material to point to ssbo memory
	mat->params = &paramsToFill;
}

void MaterialData::OnMaterialUnloaded(AssetDefs::DenseId _denseId) {
	// Reset ssbo's to defaults, in the case they are still being referenced
	mMaterialParamsBuffer.GetElement<MaterialParams>(_denseId) = {};
	mDenseIdToMaterialSlot.GetElement<AssetDefs::MaterialSlot>(_denseId) = 0;
}

void MaterialData::InitBuffers() {
	LOG_MSG("Initializing SSBO's", LogVerbosity::Info);

	constexpr uint32_t maxTextureCount = AssetManagerGlobals::AssetTraits<Texture>::config.maxCount;
	constexpr uint32_t maxCubemapCount = AssetManagerGlobals::AssetTraits<CubemapTexture>::config.maxCount;
	constexpr uint32_t maxMaterialCount = AssetManagerGlobals::AssetTraits<Material>::config.maxCount;

	mDenseIdToTextureSlot.Init(sizeof(AssetDefs::TextureSlot) * maxTextureCount);
	mDenseIdToCubemapSlot.Init(sizeof(AssetDefs::CubemapSlot) * maxCubemapCount);
	mMaterialParamsBuffer.Init(sizeof(MaterialParams) * maxMaterialCount);
	mDenseIdToMaterialSlot.Init(sizeof(AssetDefs::MaterialSlot) * maxMaterialCount);
}

void MaterialData::CreateDescriptorSet(VkDevice _logicalDevice, VkDescriptorPool _descriptorPool) {
	LOG_MSG("Creating material descriptor set", LogVerbosity::Info);

	const VkDescriptorSetLayout& layoutPreset = PipelineLayoutManager::GetInstance().GetDescriptorSetLayout(DescriptorSet::Material);

	VkDescriptorSetAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = _descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layoutPreset
	};

	if (vkAllocateDescriptorSets(_logicalDevice, &allocInfo, &mDescriptorSet) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate material descriptor set!");
	}

	// Initial SSBO mappings
	mDenseIdToTextureSlot.WriteToDescriptorSet(mDescriptorSet, 1);
	mDenseIdToCubemapSlot.WriteToDescriptorSet(mDescriptorSet, 3);
	mMaterialParamsBuffer.WriteToDescriptorSet(mDescriptorSet, 4);
	mDenseIdToMaterialSlot.WriteToDescriptorSet(mDescriptorSet, 5);
}

void MaterialData::CreateTextureSamplerForSlot(AssetDefs::TextureSlot _texSlot) {
	VkSamplerCreateInfo samplerInfo = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR, // concerns oversampling
		.minFilter = VK_FILTER_LINEAR, // concerns undersampling

		// What to do when sampling outside the texture borders
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,

		// Enable anisotropy, as it is relatively cheap nowadays
		.anisotropyEnable = VK_TRUE,

		// If a comparision function is enabled, texels are first compared
		// to some value and the result of the comparison is used for filtering
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,

		// Border color when sampling outside the texture borders
		// and address mode is 'clamp to border'
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE,

		// If true, sampler uses [0..texWidth] and [0..texHeight] for
		// sampling texture coordinates. If false, simply use [0..1]
		// Note, most applications keep this false so UVs are [0..1]
		.unnormalizedCoordinates = VK_FALSE,
	};

	// We define these here because they are not in a sensible order
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.f;
	samplerInfo.minLod = 0.f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE; // no max lod

	// Query max anisotropy supported by device
	VkPhysicalDeviceProperties properties = {};
	vkGetPhysicalDeviceProperties(App::GetInstance().GetPhysicalDevice(), &properties);

	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

	mTextureSlotToSampler[_texSlot] = {};
	if (vkCreateSampler(App::GetInstance().GetLogicalDevice(), &samplerInfo, nullptr, &mTextureSlotToSampler[_texSlot]) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create texture sampler!");
	}
}

void MaterialData::CreateCubemapSamplerForSlot(AssetDefs::CubemapSlot _texSlot) {
	VkSamplerCreateInfo samplerInfo = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR, // concerns oversampling
		.minFilter = VK_FILTER_LINEAR, // concerns undersampling

		// What to do when sampling outside the texture borders
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,

		// Enable anisotropy, as it is relatively cheap nowadays
		.anisotropyEnable = VK_TRUE,

		// If a comparision function is enabled, texels are first compared
		// to some value and the result of the comparison is used for filtering
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,

		// Border color when sampling outside the texture borders
		// and address mode is 'clamp to border'
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE,

		// If true, sampler uses [0..texWidth] and [0..texHeight] for
		// sampling texture coordinates. If false, simply use [0..1]
		// Note, most applications keep this false so UVs are [0..1]
		.unnormalizedCoordinates = VK_FALSE,
	};

	// We define these here because they are not in a sensible order
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.f;
	samplerInfo.minLod = 0.f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE; // no max lod

	// Query max anisotropy supported by device
	VkPhysicalDeviceProperties properties = {};
	vkGetPhysicalDeviceProperties(App::GetInstance().GetPhysicalDevice(), &properties);

	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

	mCubemapSlotToSampler[_texSlot] = {};
	if (vkCreateSampler(App::GetInstance().GetLogicalDevice(), &samplerInfo, nullptr, &mCubemapSlotToSampler[_texSlot]) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create cubemap sampler!");
	}
}

void MeshData::Init(VkDevice _logicalDevice, VkDescriptorPool _descriptorPool) {
	InitBuffers();
	CreateDescriptorSet(_logicalDevice, _descriptorPool);
}

void MeshData::Reset() {
	mInstanceTransforms.Reset();
	mInstToTransformIndex.Reset();
}

void MeshData::MapFreeTransform(RenderTransform& _value, uint32_t& _outIndex) {
	// Grab next free transform index
	_outIndex = mFreeTransforms.top();
	mFreeTransforms.pop();

	// Set value
	glm::mat4& newValue = mInstanceTransforms.GetElement<glm::mat4>(_outIndex);
	_value.SetMappedMatrix(newValue);
}

void MeshData::BindTransformIndices(const std::vector<uint32_t>& _transformIndices) {
	// Set each instance index to correct transform index
	for (size_t i = 0; i < _transformIndices.size(); i++) {
		// The use of 'mTransformIndexOffset' should directly match the shader code
		uint32_t& index = mInstToTransformIndex.GetElement<uint32_t>(mTransformIndexOffset + i);
		index = _transformIndices[i];
	}

	mTransformIndexOffset += static_cast<uint32_t>(_transformIndices.size());
}

void MeshData::ZeroTransformIndexOffset() {
	mTransformIndexOffset = 0;
}

void MeshData::PushTransformIndexOffset(VkCommandBuffer _commandBuffer, VkPipelineLayout _layout) const {
	vkCmdPushConstants(
		_commandBuffer,
		_layout,
		VK_SHADER_STAGE_VERTEX_BIT,
		0,
		sizeof(uint32_t),
		&mTransformIndexOffset
	);
}

void MeshData::InitBuffers() {
	LOG_MSG("Initializing SSBO's", LogVerbosity::Info);

	// These are test values, real apps would have WAY higher numbers
	constexpr uint32_t maxTransformCount = 100;

	mInstanceTransforms.Init(sizeof(glm::mat4) * maxTransformCount);
	mInstToTransformIndex.Init(sizeof(uint32_t) * maxTransformCount);

	for (uint32_t i = 0; i < maxTransformCount; i++) {
		uint32_t current = maxTransformCount - i - 1;
		mFreeTransforms.push(current);
	}
}

void MeshData::CreateDescriptorSet(VkDevice _logicalDevice, VkDescriptorPool _descriptorPool) {
	LOG_MSG("Creating mesh descriptor set", LogVerbosity::Info);

	const VkDescriptorSetLayout& layoutPreset = PipelineLayoutManager::GetInstance().GetDescriptorSetLayout(DescriptorSet::Mesh);

	VkDescriptorSetAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = _descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layoutPreset
	};

	if (vkAllocateDescriptorSets(_logicalDevice, &allocInfo, &mDescriptorSet) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate mesh descriptor set!");
	}

	// Initial SSBO mappings
	mInstanceTransforms.WriteToDescriptorSet(mDescriptorSet, 0);
	mInstToTransformIndex.WriteToDescriptorSet(mDescriptorSet, 1);
}

#pragma region RenderBucketMap
RenderBucketMap::RenderBucketMap() {
	// Init free ids
	for (RuntimeRenderId i = std::numeric_limits<RuntimeRenderId>::max(); i >= 1; i--) {
		mFreeIds.push(i - 1);
	}

	// Init buckets
	for (size_t b = 0; b < static_cast<size_t>(BlendModel::Count); b++) {
		for (size_t s = 0; s < static_cast<size_t>(ShadingModel::Count); s++) {
			RenderPassHash hash = GetRenderPassHash(static_cast<BlendModel>(b), static_cast<ShadingModel>(s));
			mBuckets.insert({ hash, {} });
		}
	}
}

RenderBucketMap::RuntimeRenderId RenderBucketMap::RegisterMeshInstance(std::shared_ptr<MeshInstance> _toRegister) {
	assert(!mFreeIds.empty());

	// Get next free id
	_toRegister->mRuntimeId = mFreeIds.top();
	mFreeIds.pop();

	// Get blend + shading models
	BlendModel blend = BlendModel::Opaque;
	ShadingModel shading = ShadingModel::Unlit;

	if (auto meshMat = _toRegister->mMaterial.lock()) {
		const Shader& shader = meshMat->GetShader();

		blend = shader.GetBlendModel();
		shading = shader.GetShadingModel();
	}

	// Store in map
	RenderPassHash hash = GetRenderPassHash(blend, shading);
	mBuckets[hash].insert({ _toRegister->mRuntimeId, _toRegister });

	return _toRegister->mRuntimeId;
}

void RenderBucketMap::UnregisterMeshInstance(std::weak_ptr<MeshInstance> _toUnregister) {
	auto meshInst = _toUnregister.lock();
	if (!meshInst) {
		return;
	}

	// Get blend + shading models
	BlendModel blend = BlendModel::Opaque;
	ShadingModel shading = ShadingModel::Unlit;

	if (auto meshMat = meshInst->mMaterial.lock()) {
		const Shader& shader = meshMat->GetShader();

		blend = shader.GetBlendModel();
		shading = shader.GetShadingModel();
	}

	// Erase from map
	UnregisterId(
		GetRenderPassHash(blend, shading), 
		meshInst->GetRuntimeId()
	);
}

void RenderBucketMap::UnregisterMeshInstance(const RuntimeRenderId& _toUnregisterId) {
	// Find bucket that contains the id, then unregister it
	for (auto& [hash, bucket] : mBuckets) {
		if (bucket.contains(_toUnregisterId)) {
			UnregisterId(hash, _toUnregisterId);
			break;
		}
	}
}

void RenderBucketMap::UnregisterId(RenderPassHash _bucketHash, RuntimeRenderId _id) {
	mBuckets[_bucketHash].erase(_id);
	mFreeIds.push(_id);
}
#pragma endregion
