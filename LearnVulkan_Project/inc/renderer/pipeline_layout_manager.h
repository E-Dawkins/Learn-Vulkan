#pragma once
#include "utils/singleton.h"

#include <array>

enum class BlendModel
{
	Opaque
};

enum class ShadingModel
{
	Unlit
};

struct PipelinePreset
{
	BlendModel blend;
	ShadingModel shading;
	VkPipelineColorBlendAttachmentState colorBlendAttachment;
	VkPipelineColorBlendStateCreateInfo colorBlendState;
	VkPipelineDepthStencilStateCreateInfo depthStencilState;
	uint32_t renderPassIndex;
	uint32_t subpassIndex;
};

struct DescriptorSetLayout
{
	struct SetLayoutBinding
	{
		VkDescriptorType type;
		VkShaderStageFlags stage;
	};

	std::vector<SetLayoutBinding> layoutBindings;
};

class PipelineLayoutManager : public ISingleton<PipelineLayoutManager>
{
private:
	std::vector<PipelinePreset> mPresets;
	std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts;
	std::unordered_map<size_t, std::pair<VkPipelineLayout, uint8_t>> mLayouts;

public:
	PipelineLayoutManager();
	~PipelineLayoutManager();

private:
	void InitPresets();
	void CreateDescriptorSetLayouts();
	void CreatePresetLayouts();

	size_t GetModelHash(BlendModel _blend, ShadingModel _shading) const;

public:
	const VkPipelineLayout& GetLayoutForModel(BlendModel _blend, ShadingModel _shading) const;
	const PipelinePreset& GetPresetForModel(BlendModel _blend, ShadingModel _shading) const;
};