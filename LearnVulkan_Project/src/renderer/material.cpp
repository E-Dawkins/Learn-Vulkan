#include "pch.h"
#include "renderer/material.h"

#include "renderer/shader.h"

Material::Material(Shader* _shader, MaterialParams _params) {
	assert(_shader);

	mShader = _shader;
	params = _params;
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
