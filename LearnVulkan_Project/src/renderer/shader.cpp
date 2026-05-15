#include "pch.h"

#include "app.h"
#include "renderer/mesh.h"
#include "renderer/shader.h"

Shader::Shader(const std::vector<ShaderStage>& _stages, BlendModel _blendModel, ShadingModel _shadingModel, RasterizerState _rasterizerState)
	: mBlendModel(_blendModel), mShadingModel(_shadingModel), mRasterizerState(_rasterizerState) 
{
	std::vector<VkShaderModule> activeModules;
	std::vector<VkPipelineShaderStageCreateInfo> stageCreateInfos;

	for (auto& stage : _stages) {
		EnforceFileExtension(stage.filePath, ".spv");

		std::vector<char> fileContents;
		ReadWholeFile(stage.filePath, std::ios::ate | std::ios::binary, fileContents);

		activeModules.emplace_back(CreateModule(stage, fileContents));

		stageCreateInfos.emplace_back(VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = stage.flagBit,
			.module = activeModules.back(),
			.pName = stage.entryPoint.c_str(),
		});
	}

	CreatePipeline(stageCreateInfos);

	// Destroy shader modules as they are no longer used
	const VkDevice& logicalDevice = App::GetInstance().GetLogicalDevice();

	for (auto& module : activeModules) {
		vkDestroyShaderModule(logicalDevice, module, nullptr);
	}
	activeModules.clear();

	stageCreateInfos.clear();
}

Shader::~Shader() {
	vkDestroyPipeline(App::GetInstance().GetLogicalDevice(), mPipeline, nullptr);
}

void Shader::BindShaderResources(const VkCommandBuffer& _commandBuffer) const {
	vkCmdBindPipeline(_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
}

VkShaderModule Shader::CreateModule(const ShaderStage& _stage, const std::vector<char>& _shaderCode) {
	VkShaderModuleCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = _shaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t*>(_shaderCode.data())
	};

	VkShaderModule shaderModule = {};
	if (vkCreateShaderModule(App::GetInstance().GetLogicalDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create shader module for: " + _stage.filePath.string());
	}

	return shaderModule;
}

void Shader::CreatePipeline(const std::vector<VkPipelineShaderStageCreateInfo>& _stageCreateInfos) {
	const App& appInst = App::GetInstance();

	const PipelineLayoutManager& pipelineLayoutManagerInst = PipelineLayoutManager::GetInstance();
	const PipelinePreset& preset = pipelineLayoutManagerInst.GetPresetForModel(mBlendModel, mShadingModel);

	// -- Vertex Input --
	auto bindingDescription = Vertex::GetBindingDescription();
	auto attributeDescriptions = Vertex::GetAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
		.pVertexAttributeDescriptions = attributeDescriptions.data()
	};

	// -- Input Assembly --
	// Say the vertex data is only triangles, no primitive restart
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE
	};

	// -- Dynamic States --
	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data()
	};

	// -- Viewport State (no viewport or scissor pointers) --
	VkPipelineViewportStateCreateInfo viewportState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};

	// -- Rasterizer --
	VkPipelineRasterizationStateCreateInfo rasterizer = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE, // do not clamp fragment depths, discard them
		.rasterizerDiscardEnable = VK_FALSE, // do not discard rasterizer stage
		.polygonMode = mRasterizerState.polygonMode,
		.cullMode = mRasterizerState.cullMode,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.lineWidth = mRasterizerState.lineWidth
	};

	// -- Multisampling --
	VkPipelineMultisampleStateCreateInfo multisampling = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = appInst.GetMsaaState().samples,
		.sampleShadingEnable = appInst.GetMsaaState().sampleShadingEnabled,
		.minSampleShading = appInst.GetMsaaState().minSampleShading,
	};

	VkGraphicsPipelineCreateInfo pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = static_cast<uint32_t>(_stageCreateInfos.size()),
		.pStages = _stageCreateInfos.data(),
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &preset.depthStencilState,
		.pColorBlendState = &preset.colorBlendState,
		.pDynamicState = &dynamicState,
		.layout = pipelineLayoutManagerInst.GetLayoutForModel(mBlendModel, mShadingModel),
		.renderPass = appInst.GetRenderPass(static_cast<size_t>(preset.shading)),
		.subpass = static_cast<uint32_t>(preset.shading),
	};

	// This function call can actually be used to create multiple
	// pipelines at once, by using an array of pipeline info structs
	if (vkCreateGraphicsPipelines(appInst.GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mPipeline) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pipeline!");
	}
}
