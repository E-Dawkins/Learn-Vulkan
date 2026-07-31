#pragma once
#include "interfaces/singleton.h"

#include "renderer/renderer_types.h"

constexpr uint32_t gPcRangeVertex = 64;

enum class DescriptorSet
{
	Global,
	Material,
	Mesh
};

struct PipelinePreset
{
	BlendModel blend;
	ShadingModel shading;
	VkPipelineColorBlendAttachmentState colorBlendAttachment;
	VkPipelineColorBlendStateCreateInfo colorBlendState;
	VkPipelineDepthStencilStateCreateInfo depthStencilState;
};

struct DescriptorSetLayout
{
	struct SetLayoutBinding
	{
		VkDescriptorType type;
		VkShaderStageFlags stage;
		uint32_t count = 1;
		VkDescriptorBindingFlags bindingFlags;
	};

	std::vector<SetLayoutBinding> layoutBindings;
	VkDescriptorSetLayoutCreateFlags layoutCreateFlags;
};

class PipelineLayoutManager : public ISingleton<PipelineLayoutManager>
{
private:
	std::vector<PipelinePreset> mPresets;
	std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts;
	std::unordered_map<size_t, std::pair<VkPipelineLayout, uint8_t>> mLayouts;

private:
	void OnInitialized() override;
	void OnCleanup() override;

	void InitPresets();
	void CreateDescriptorSetLayouts();
	void CreatePresetLayouts();

	size_t GetModelHash(BlendModel _blend, ShadingModel _shading) const;

public:
	const VkPipelineLayout& GetLayoutForModel(BlendModel _blend, ShadingModel _shading) const;
	const PipelinePreset& GetPresetForModel(BlendModel _blend, ShadingModel _shading) const;
	const VkDescriptorSetLayout& GetDescriptorSetLayout(DescriptorSet _set) const;
};