#pragma once
#include <cstdint>

class Shader;

struct MaterialParams
{
	uint32_t runtimeTexIds[8];
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