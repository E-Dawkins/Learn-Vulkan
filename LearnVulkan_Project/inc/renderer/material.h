#pragma once
#include "utils/asset.h"
#include "utils/file_reader.h"
#include "utils/type_defs.h"

class Shader;

struct MaterialParams
{
	AssetDefs::DenseId denseTexIds[8];
	glm::vec4 colorVars[6];
};

class Material : IFileReader, public IAsset
{
private:
	Shader* mShader;

public:
	MaterialParams params;

public:
	Material(const std::filesystem::path& _filepath);
	~Material();

	void BindMaterialResources(const VkCommandBuffer& _commandBuffer);

private:
	Shader* CreateShaderFromAil(const AilReader& _reader);
	void FillParamsFromAil(const AilReader& _reader);
};