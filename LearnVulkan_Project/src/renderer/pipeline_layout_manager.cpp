#include "pch.h"
#include "renderer/pipeline_layout_manager.h"

#include "app.h"
#include "renderer/asset_manager.h"
#include "utils/debug_logger.h"
#include "utils/hash_utils.h"

void PipelineLayoutManager::OnInitialized() {
	LOG_MSG("Init", LogVerbosity::Info);

	InitPresets();
	CreateDescriptorSetLayouts();
	CreatePresetLayouts();
}

void PipelineLayoutManager::OnCleanup() {
	const auto& logicalDevice = App::GetInstance().GetLogicalDevice();

	for (const auto& [hash, pipelineIndexPair] : mLayouts) {
		vkDestroyPipelineLayout(logicalDevice, pipelineIndexPair.first, nullptr);
	}
	mLayouts.clear();

	for (size_t i = 0; i < mDescriptorSetLayouts.size(); i++) {
		vkDestroyDescriptorSetLayout(logicalDevice, mDescriptorSetLayouts[i], nullptr);
	}
	mDescriptorSetLayouts.clear();

	mPresets.clear();

	LOG_MSG("Cleanup", LogVerbosity::Info);
}

void PipelineLayoutManager::InitPresets() {
	// Opaque - Unlit
	mPresets.emplace_back(PipelinePreset{
		.blend = BlendModel::Opaque,
		.shading = ShadingModel::Unlit,
		.colorBlendAttachment = { // no blending for now
			.blendEnable = VK_FALSE,
			.colorWriteMask =
				VK_COLOR_COMPONENT_R_BIT |
				VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT |
				VK_COLOR_COMPONENT_A_BIT,
		},
		.depthStencilState = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
		}
	});

	auto& back = mPresets.back();
	back.colorBlendState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.attachmentCount = 1,
		.pAttachments = &back.colorBlendAttachment,
	};
}

void PipelineLayoutManager::CreateDescriptorSetLayouts() {
	std::vector<DescriptorSetLayout> setLayouts;

	// Set 0 - globals
	setLayouts.emplace_back(DescriptorSetLayout{
		.layoutBindings{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT } // cameraData
		}
	});

	// Set 1 - material system
	setLayouts.emplace_back(DescriptorSetLayout{
		.layoutBindings{
			//{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT }, // materialParams[]
			// texSamplers[]
			{
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
				VK_SHADER_STAGE_FRAGMENT_BIT, 
				AssetManagerGlobals::AssetTraits<Texture>::config.maxCount,
				VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT, // binding flags
			},
			// denseIdToTexSlot[]
			{ 
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
				VK_SHADER_STAGE_FRAGMENT_BIT 
			},
			// materialParams[]
			{
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				VK_SHADER_STAGE_FRAGMENT_BIT
			},
			// denseIdToMatSlot[]
			{
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				VK_SHADER_STAGE_FRAGMENT_BIT
			}
		},
		.layoutCreateFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT
	});

	// Set 2 - mesh data
	//setLayouts.emplace_back(DescriptorSetLayout{
	//	.layoutBindings{
	//		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT }, // meshTransform
	//	}
	//});

	for (const auto& layout : setLayouts) {
		std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
		std::vector<VkDescriptorBindingFlags> layoutBindingFlags;

		for (const auto& binding : layout.layoutBindings) {
			layoutBindings.emplace_back(VkDescriptorSetLayoutBinding{
				.binding = static_cast<uint32_t>(layoutBindings.size()),
				.descriptorType = binding.type,
				.descriptorCount = binding.count,
				.stageFlags = binding.stage
			});

			layoutBindingFlags.push_back(binding.bindingFlags);
		}

		VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingsFlags = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.bindingCount = static_cast<uint32_t>(layoutBindingFlags.size()),
			.pBindingFlags = layoutBindingFlags.data(),
		};

		VkDescriptorSetLayoutCreateInfo layoutInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,

			// Apply flags for each binding
			.pNext = &setLayoutBindingsFlags,

			// Apply flags to the set layout
			.flags = layout.layoutCreateFlags,

			.bindingCount = static_cast<uint32_t>(layoutBindings.size()),
			.pBindings = layoutBindings.data()
		};

		mDescriptorSetLayouts.push_back({});

		if (vkCreateDescriptorSetLayout(App::GetInstance().GetLogicalDevice(), &layoutInfo, nullptr, &mDescriptorSetLayouts.back()) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create descriptor set layout: " + std::to_string(mDescriptorSetLayouts.size()));
		}
	}
}

void PipelineLayoutManager::CreatePresetLayouts() {
	// Setup all pipeline layouts to have some amount of push constant range
	// For now, we just set it up to have the minimum guaranteed amount
	// Guaranteed 128-bytes total by Vulkan spec
	const std::array<VkPushConstantRange, 2> pcRanges{
		VkPushConstantRange{ .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = 64 },
		VkPushConstantRange{ .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 64, .size = 64 },
	};

	for (uint8_t i = 0; i < mPresets.size(); i++) {
		// This is where descriptor sets / uniforms are bound to the pipeline
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = static_cast<uint32_t>(mDescriptorSetLayouts.size()),
			.pSetLayouts = mDescriptorSetLayouts.data(),
			.pushConstantRangeCount = static_cast<uint32_t>(pcRanges.size()),
			.pPushConstantRanges = pcRanges.data()
		};

		const PipelinePreset& preset = mPresets[i];

		size_t hash = GetModelHash(preset.blend, preset.shading);
		mLayouts[hash] = std::make_pair(VkPipelineLayout(), i);

		if (vkCreatePipelineLayout(App::GetInstance().GetLogicalDevice(), &pipelineLayoutInfo, nullptr, &mLayouts[hash].first) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create pipeline layout!");
		}
	}
}

size_t PipelineLayoutManager::GetModelHash(BlendModel _blend, ShadingModel _shading) const {
	size_t seed = 0;

	Utils::HashUtils::HashCombine(seed, static_cast<size_t>(_blend));
	Utils::HashUtils::HashCombine(seed, static_cast<size_t>(_shading));
	
	return seed;
}

const VkPipelineLayout& PipelineLayoutManager::GetLayoutForModel(BlendModel _blend, ShadingModel _shading) const {
	const size_t hash = GetModelHash(_blend, _shading);
	assert(mLayouts.contains(hash));

	return mLayouts.at(hash).first;
}

const PipelinePreset& PipelineLayoutManager::GetPresetForModel(BlendModel _blend, ShadingModel _shading) const {
	const size_t hash = GetModelHash(_blend, _shading);
	assert(mLayouts.contains(hash));

	const uint8_t presetIndex = mLayouts.at(hash).second;
	assert(presetIndex < mPresets.size());

	return mPresets[presetIndex];
}

const VkDescriptorSetLayout& PipelineLayoutManager::GetDescriptorSetLayout(DescriptorSet _set) const {
	uint8_t setIndex = static_cast<uint8_t>(_set);

	assert(setIndex < mDescriptorSetLayouts.size());

	return mDescriptorSetLayouts[setIndex];
}
