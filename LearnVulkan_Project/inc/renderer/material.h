#pragma once
#include "interfaces/asset.h"
#include "interfaces/file_reader.h"
#include "utils/type_defs.h"

class Shader;

struct MaterialParams
{
	AssetDefs::DenseId denseTexIds[8];
	glm::vec4 colorVars[8];
	int32_t intVars[8];
};

class Material : IFileReader, public IAsset
{
private:
	Shader* mShader;

public:
	MaterialParams* params = nullptr;

public:
	Material(const std::filesystem::path& _filepath);
	~Material();

	void BindMaterialResources(const VkCommandBuffer& _commandBuffer);
	const std::string DebugStr() const;

private:
	Shader* CreateShaderFromAil(const AilReader& _reader) const;
	void FillParamsFromAil(const AilReader& _reader);
};