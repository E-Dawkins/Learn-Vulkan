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
	auto warn_user = [this](const std::string& _s) {
		std::cerr << "WARNING: No " << _s << " found on disk for " << DebugStr() << "\n";
	};

	AilNode shaderNode;
	if (!_reader.TryGetNode("shader", shaderNode)) {
		warn_user("shader params");
		return nullptr;
	}

	std::vector<ShaderStage> shaderStages;
	if (AilNode shaderStagesArray; shaderNode.TryGetSubnode("shaderStages", shaderStagesArray)) {
		for (const AilNode& stageNode : shaderStagesArray.subnodes) {
			AilNode pathNode, flagNode;

			if (!stageNode.TryGetSubnode(0, pathNode)) continue;
			if (!stageNode.TryGetSubnode(1, flagNode)) continue;

			shaderStages.emplace_back(ShaderStage{
				.filePath = pathNode.GetAsStr(),
				.flagBit = flagNode.GetAsSizetCasted<VkShaderStageFlagBits>()
			});
		}
	}
	else {
		warn_user("shaderStages");
		return nullptr;
	}

	RasterizerState rasterizerState;
	if (AilNode rasterizerStateNode; shaderNode.TryGetSubnode("rasterizerState", rasterizerStateNode)) {
		AilNode polygonNode, cullNode, lineNode;

		if (rasterizerStateNode.TryGetSubnode("polygonMode", polygonNode)) {
			rasterizerState.polygonMode = polygonNode.GetAsSizetCasted<VkPolygonMode>();
		}

		if (rasterizerStateNode.TryGetSubnode("cullMode", cullNode)) {
			rasterizerState.cullMode = cullNode.GetAsSizetCasted<VkCullModeFlags>();
		}

		if (rasterizerStateNode.TryGetSubnode("lineWidth", lineNode)) {
			rasterizerState.lineWidth = lineNode.Get<float>();
		}
	}
	else {
		warn_user("rasterizerState");
		return nullptr;
	}

	AilNode blendNode, shadingNode;
	shaderNode.TryGetSubnode("blendModel", blendNode);
	shaderNode.TryGetSubnode("shadingModel", shadingNode);

	Shader* out = new Shader(
		shaderStages,
		blendNode.GetAsSizetCasted<BlendModel>(),
		shadingNode.GetAsSizetCasted<ShadingModel>(),
		rasterizerState
	);

	return out;
}

void Material::FillParamsFromAil(const AilReader& _reader) {
	auto warn_user = [this](const std::string& _s) {
		std::cerr << "WARNING: No " << _s << " found on disk for " << DebugStr() << "\n";
	};

	if (!params) {
		params = new MaterialParams();
	}

	AilNode paramsNode;
	if (!_reader.TryGetNode("materialParams", paramsNode)) {
		warn_user("materialParams");
		return;
	}

	if (AilNode stableTexIdsNode; paramsNode.TryGetSubnode("stableTexIds", stableTexIdsNode)) {
		for (size_t i = 0; i < 8; i++) {
			AilNode idNode;
			if (!stableTexIdsNode.TryGetSubnode(i, idNode)) continue;

			AssetDefs::StableId stableId = idNode.GetAsSizetCasted<AssetDefs::StableId>();

			const AssetManager& manager = AssetManager::GetInstance();
			if (manager.IsAssetLoaded(stableId)) {
				params->denseTexIds[i] = manager.GetAssetFromStableId<Texture>(stableId).GetDenseId();
			}
		}
	}
	else {
		warn_user("stableTexIds");
	}

	if (AilNode colorVarsNode; paramsNode.TryGetSubnode("colorVars", colorVarsNode)) {
		for (size_t i = 0; i < 8; i++) {
			AilNode colorNode;
			if (!colorVarsNode.TryGetSubnode(i, colorNode)) continue;

			params->colorVars[i] = colorNode.Get<glm::vec4>();
		}
	}
	else {
		warn_user("colorVars");
	}

	if (AilNode intVarsNode; paramsNode.TryGetSubnode("intVars", intVarsNode)) {
		for (size_t i = 0; i < 8; i++) {
			AilNode intNode;
			if (!intVarsNode.TryGetSubnode(i, intNode)) continue;

			params->intVars[i] = intNode.GetAsSizetCasted<int32_t>();
		}
	}
	else {
		warn_user("intVars");
	}
}
