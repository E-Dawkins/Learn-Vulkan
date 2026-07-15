#include "pch.h"
#include "renderer/material.h"

#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/asset_manager.h"

Material::Material(const std::filesystem::path& _filepath) : IAsset(_filepath) {
	EnforceFileExtension(_filepath, ".material");

	AilReader reader;
	ReadFileAsAil(_filepath, reader);

	CreateShaderFromAil(reader);
	FillParamsFromAil(reader);
}

Material::~Material() {
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
		VK_SHADER_STAGE_FRAGMENT_BIT,
		64, // TODO - make this not hard-coded (fragment pc range starts at offset 64)
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

void Material::CreateShaderFromAil(const AilReader& _reader) {
	auto warn_user = [this](const std::string& _s) {
		std::cerr << "WARNING: No " << _s << " found on disk for " << DebugStr() << "\n";
	};

	auto shaderNode = _reader.TryGetNode("shader");
	if (!shaderNode) {
		warn_user("shader params");
		return;
	}

	std::vector<ShaderStage> shaderStages;
	if (auto shaderStagesArray = shaderNode->TryGetSubnode("shaderStages")) {
		for (const AilNode& stageNode : shaderStagesArray->subnodes) {
			shaderStages.emplace_back(ShaderStage());

			if (auto pathNode = stageNode.TryGetSubnode(0)) {
				shaderStages.back().filePath = pathNode->GetAsStr();
			}

			if (auto flagNode = stageNode.TryGetSubnode(1)) {
				shaderStages.back().flagBit = flagNode->GetAsSizetCasted<VkShaderStageFlagBits>();
			}
		}
	}
	else {
		warn_user("shaderStages");
		return;
	}

	RasterizerState rasterizerState;
	if (auto rasterizerStateNode = shaderNode->TryGetSubnode("rasterizerState")) {
		if (auto polygonNode = rasterizerStateNode->TryGetSubnode("polygonMode")) {
			rasterizerState.polygonMode = polygonNode->GetAsSizetCasted<VkPolygonMode>();
		}

		if (auto cullNode = rasterizerStateNode->TryGetSubnode("cullMode")) {
			rasterizerState.cullMode = cullNode->GetAsSizetCasted<VkCullModeFlags>();
		}

		if (auto lineNode = rasterizerStateNode->TryGetSubnode("lineWidth")) {
			rasterizerState.lineWidth = lineNode->Get<float>();
		}
	}
	else {
		warn_user("rasterizerState");
		return;
	}

	BlendModel blendModel = BlendModel::Opaque;
	ShadingModel shadingModel = ShadingModel::Unlit;

	if (auto blendNode = shaderNode->TryGetSubnode("blendModel")) {
		blendModel = blendNode->GetAsSizetCasted<BlendModel>();
	}
	if (auto shadingNode = shaderNode->TryGetSubnode("shadingModel")) {
		shadingModel = shadingNode->GetAsSizetCasted<ShadingModel>();
	}

	mShader = std::make_unique<Shader>(
		shaderStages,
		blendModel,
		shadingModel,
		rasterizerState
	);
}

void Material::FillParamsFromAil(const AilReader& _reader) {
	auto warn_user = [this](const std::string& _s) {
		std::cerr << "WARNING: No " << _s << " found on disk for " << DebugStr() << "\n";
	};

	if (!params) {
		params = new MaterialParams();
	}

	auto paramsNode = _reader.TryGetNode("materialParams");
	if (!paramsNode) {
		warn_user("materialParams");
		return;
	}

	if (auto stableTexIdsNode = paramsNode->TryGetSubnode("stableTexIds")) {
		for (size_t i = 0; i < 8; i++) {
			auto idNode = stableTexIdsNode->TryGetSubnode(i);
			if (!idNode) continue;

			AssetDefs::StableId stableId = idNode->GetAsSizetCasted<AssetDefs::StableId>();

			const AssetManager& manager = AssetManager::GetInstance();
			if (manager.IsAssetLoaded(stableId)) {
				if (auto tex = manager.GetAsset<Texture>(stableId).lock()) {
					params->denseTexIds[i] = tex->GetDenseId();
				}
			}
		}
	}
	else {
		warn_user("stableTexIds");
	}

	if (auto stableCubemapIdsNode = paramsNode->TryGetSubnode("stableCubemapIds")) {
		for (size_t i = 0; i < 8; i++) {
			auto idNode = stableCubemapIdsNode->TryGetSubnode(i);
			if (!idNode) continue;

			AssetDefs::StableId stableId = idNode->GetAsSizetCasted<AssetDefs::StableId>();

			const AssetManager& manager = AssetManager::GetInstance();
			if (manager.IsAssetLoaded(stableId)) {
				if (auto tex = manager.GetAsset<CubemapTexture>(stableId).lock()) {
					params->denseCubemapIds[i] = tex->GetDenseId();
				}
			}
		}
	}
	else {
		warn_user("stableCubemapIds");
	}

	if (auto colorVarsNode = paramsNode->TryGetSubnode("colorVars")) {
		for (size_t i = 0; i < 8; i++) {
			auto colorNode = colorVarsNode->TryGetSubnode(i);
			if (!colorNode) continue;

			params->colorVars[i] = colorNode->Get<glm::vec4>();
		}
	}
	else {
		warn_user("colorVars");
	}

	if (auto intVarsNode = paramsNode->TryGetSubnode("intVars")) {
		for (size_t i = 0; i < 8; i++) {
			auto intNode = intVarsNode->TryGetSubnode(i);
			if (!intNode) continue;

			params->intVars[i] = intNode->GetAsSizetCasted<int32_t>();
		}
	}
	else {
		warn_user("intVars");
	}
}
