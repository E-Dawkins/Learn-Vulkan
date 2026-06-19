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
	std::unique_ptr<Shader> mShader;

public:
	MaterialParams* params = nullptr; // eventually will point to GPU-side memory

public:
	Material(const std::filesystem::path& _filepath);
	~Material();

	void BindMaterialResources(const VkCommandBuffer& _commandBuffer);
	const std::string DebugStr() const;

	inline const Shader& GetShader() const { return *mShader; }

private:
	void CreateShaderFromAil(const AilReader& _reader);
	void FillParamsFromAil(const AilReader& _reader);
};