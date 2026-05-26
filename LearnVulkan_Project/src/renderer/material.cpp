#include "pch.h"
#include "renderer/material.h"

#include "renderer/shader.h"
#include "utils/asset_manager.h"

Material::Material(const std::filesystem::path& _filepath) : IAsset(_filepath) {
	EnforceFileExtension(_filepath, ".material");

	AilReader reader;
	ReadFileAsAil(_filepath, reader);

	mShader = CreateShaderFromAil(reader);
	FillParamsFromAil(reader);
}

Material::~Material() {
	delete mShader;
}

void Material::BindMaterialResources(const VkCommandBuffer& _commandBuffer) {
	mShader->BindShaderResources(_commandBuffer);

	// Bind material params as push constants ... for now
	vkCmdPushConstants(
		_commandBuffer,
		mShader->GetLayoutForShader(),
		VK_SHADER_STAGE_ALL_GRAPHICS,
		0,
		sizeof(MaterialParams),
		&params
	);
}

Shader* Material::CreateShaderFromAil(const AilReader& _reader) {
	AilNode* shaderStagesArray = _reader.GetNode("shader|shaderStage");
	assert(shaderStagesArray);

	std::vector<ShaderStage> shaderStages;
	for (size_t i = 0; i < shaderStagesArray->subnodes.size(); i++) {
		std::string path = _reader.GetAsStr("shader|shaderStage|" + std::to_string(i) + "|0");
		VkShaderStageFlagBits flag = _reader.GetAsIntCasted<VkShaderStageFlagBits>("shader|shaderStage|" + std::to_string(i) + "|1");

		shaderStages.emplace_back(ShaderStage{ .filePath = path, .flagBit = flag });
	}

	mShader = new Shader(
		shaderStages,
		_reader.GetAsIntCasted<BlendModel>("shader|blendModel"),
		_reader.GetAsIntCasted<ShadingModel>("shader|shadingModel"),
		RasterizerState{
			.polygonMode = _reader.GetAsIntCasted<VkPolygonMode>("shader|rasterizerState|polygonMode"),
			.cullMode = _reader.GetAsIntCasted<VkCullModeFlags>("shader|rasterizerState|cullMode"),
			.lineWidth = _reader.GetAsFloat("shader|rasterizerState|lineWidth"),
		}
	);

	return mShader;
}

void Material::FillParamsFromAil(const AilReader& _reader) {
	AilNode* stableIdArray = _reader.GetNode("materialParams|stableTexIds");
	assert(stableIdArray);

	for (size_t i = 0; i < stableIdArray->subnodes.size(); i++) {
		AssetDefs::StableId stableId = _reader.GetAsIntCasted<AssetDefs::StableId>("materialParams|stableTexIds|" + std::to_string(i));
		params.denseTexIds[i] = AssetManager::GetInstance().GetDenseIdForStableId(stableId);
	}

	// TODO: remove this!
	params.denseTexIds[1] = 0;

	for (size_t i = 0; i < 6; i++) {
		params.colorVars[i] = _reader.GetAsVec4("materialParams|colorVars|" + std::to_string(i));
	}
}
