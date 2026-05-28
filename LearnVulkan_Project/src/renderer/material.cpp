#include "pch.h"
#include "renderer/material.h"

#include "renderer/shader.h"
#include "renderer/texture.h"
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
	mShader = nullptr;

	// We do not need to free params* as it points to GPU memory
	// and will be freed at some point by the renderer
	// delete params;
	// We do need to nullify it though, incase something tries to use it still
	params = nullptr;
}

void Material::BindMaterialResources(const VkCommandBuffer& _commandBuffer) {
	if (!mShader) {
		throw std::runtime_error("Shader invalid for " + DebugStr() + "\n");
	}

	mShader->BindShaderResources(_commandBuffer);

	vkCmdPushConstants(
		_commandBuffer,
		mShader->GetLayoutForShader(),
		VK_SHADER_STAGE_ALL_GRAPHICS,
		0,
		sizeof(AssetDefs::DenseId),
		&GetDenseId()
	);
}

const std::string Material::DebugStr() const {
	return std::format(
		"Material [stableId: {}]",
		GetStableId()
	);
}

Shader* Material::CreateShaderFromAil(const AilReader& _reader) const {
	auto WarnUser = [this](const std::string& _s) {
		std::cerr << "WARNING: No " << _s << " found on disk for " << DebugStr() << "\n";
	};

	AilNode* shaderNode = _reader.GetNode("shader");
	if (!shaderNode) {
		WarnUser("shader params");
		return nullptr;
	}

	AilNode* shaderStagesArray = shaderNode->GetSubnode("shaderStages");
	std::vector<ShaderStage> shaderStages;
	if (shaderStagesArray) {
		for (AilNode* stageNode : *shaderStagesArray) {
			std::string path = stageNode->GetSubnode(0)->GetAsStr();
			VkShaderStageFlagBits flag = stageNode->GetSubnode(1)->GetAsSizetCasted<VkShaderStageFlagBits>();

			shaderStages.emplace_back(ShaderStage{ .filePath = path, .flagBit = flag });
		}
	}
	else {
		WarnUser("shaderStages");
		return nullptr;
	}

	AilNode* rasterizerStateNode = shaderNode->GetSubnode("rasterizerState");
	RasterizerState rasterizerState;
	if (rasterizerStateNode) {
		rasterizerState = {
			.polygonMode = rasterizerStateNode->GetSubnode("polygonMode")->GetAsSizetCasted<VkPolygonMode>(),
			.cullMode = rasterizerStateNode->GetSubnode("cullMode")->GetAsSizetCasted<VkCullModeFlags>(),
			.lineWidth = rasterizerStateNode->GetSubnode("lineWidth")->Get<float>()
		};
	}
	else {
		WarnUser("rasterizerState");
		return nullptr;
	}

	Shader* out = new Shader(
		shaderStages,
		shaderNode->GetSubnode("blendModel")->GetAsSizetCasted<BlendModel>(),
		shaderNode->GetSubnode("shadingModel")->GetAsSizetCasted<ShadingModel>(),
		rasterizerState
	);

	return out;
}

void Material::FillParamsFromAil(const AilReader& _reader) {
	auto WarnUser = [this](const std::string& _s) {
		std::cerr << "WARNING: No " << _s << " found on disk for " << DebugStr() << "\n";
	};

	if (!params) {
		params = new MaterialParams();
	}

	AilNode* paramsNode = _reader.GetNode("materialParams");
	if (!paramsNode) {
		WarnUser("materialParams");
		return;
	}

	if (AilNode* stableTexIdsNode = paramsNode->GetSubnode("stableTexIds")) {
		for (size_t i = 0; i < 8; i++) {
			AilNode* idNode = stableTexIdsNode->GetSubnode(i);
			if (!idNode) continue;

			AssetDefs::StableId stableId = idNode->GetAsSizetCasted<AssetDefs::StableId>();

			const AssetManager& manager = AssetManager::GetInstance();
			if (manager.IsAssetLoaded(stableId)) {
				params->denseTexIds[i] = manager.GetAssetFromStableId<Texture>(stableId).GetDenseId();
			}
		}
	}
	else {
		WarnUser("stableTexIds");
	}

	if (AilNode* colorVarsNode = paramsNode->GetSubnode("colorVars")) {
		for (size_t i = 0; i < 8; i++) {
			AilNode* colorNode = colorVarsNode->GetSubnode(i);
			if (!colorNode) continue;

			params->colorVars[i] = colorNode->Get<glm::vec4>();
		}
	}
	else {
		WarnUser("colorVars");
	}

	if (AilNode* intVarsNode = paramsNode->GetSubnode("intVars")) {
		for (size_t i = 0; i < 8; i++) {
			AilNode* intNode = intVarsNode->GetSubnode(i);
			if (!intNode) continue;

			params->intVars[i] = intNode->GetAsSizetCasted<int32_t>();
		}
	}
	else {
		WarnUser("intVars");
	}
}
