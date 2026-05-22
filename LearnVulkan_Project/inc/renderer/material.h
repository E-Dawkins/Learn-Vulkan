#pragma once
#include "utils/type_defs.h"

class Shader;

struct MaterialParams
{
	AssetDefs::DenseId denseTexIds[8];
	glm::vec4 colorVars[6];
};

class Material
{
private:
	Shader* mShader;

public:
	MaterialParams params;

public:
	Material(Shader* _shader, MaterialParams _params = {});
	~Material();

	void BindMaterialResources(const VkCommandBuffer& _commandBuffer);
};