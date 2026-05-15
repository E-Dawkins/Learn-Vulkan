#pragma once
#include "utils/file_reader.h"

#include "renderer/pipeline_layout_manager.h"

#include <vulkan/vulkan.h>

struct ShaderStage
{
	std::filesystem::path filePath;
	VkShaderStageFlagBits flagBit;
	std::string entryPoint = "main";
};

struct RasterizerState
{
	VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
	VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
	float lineWidth = 1.f;
};

class Shader : IFileReader
{
private:
	RasterizerState mRasterizerState;
	BlendModel mBlendModel;
	ShadingModel mShadingModel;
	VkPipeline mPipeline;

public:
	Shader(const std::vector<ShaderStage>& _stages, BlendModel _blendModel, ShadingModel _shadingModel, RasterizerState _rasterizerState = {});
	~Shader();

	void BindShaderResources(const VkCommandBuffer& _commandBuffer) const;

private:
	VkShaderModule CreateModule(const ShaderStage& _stage, const std::vector<char>& _shaderCode);
	void CreatePipeline(const std::vector<VkPipelineShaderStageCreateInfo>& _stageCreateInfos);
};