#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (set = 1, binding = 0) uniform sampler2D textures[];
layout (set = 1, binding = 1) readonly buffer DenseIdToTextureSlot {
	uint texSlots[];
};

layout (set = 1, binding = 2) uniform samplerCube cubemaps[];
layout (set = 1, binding = 3) readonly buffer DenseIdToCubemapSlot {
	uint cubemapSlots[];
};

struct MaterialParams
{
	uint denseTexIds[8];
	uint denseCubemapIds[8];
	vec4 colorVars[8];
	int intVars[8];
};

layout (set = 1, binding = 4) readonly buffer MaterialParamsBuffer {
	MaterialParams matParams[];
};

layout (set = 1, binding = 5) readonly buffer DenseIdToMaterialSlot {
	uint matSlots[];
};

layout(push_constant) uniform PushConstants {
	layout(offset = 64) uint denseMatSlotId;
} pc;

layout (location = 0) in vec3 iDirection;

layout (location = 0) out vec4 oColor;

void main() {
	uint matSlot = matSlots[pc.denseMatSlotId];
	MaterialParams params = matParams[matSlot];

	uint cubemapSlot = cubemapSlots[params.denseCubemapIds[0]];

	vec3 dir = normalize(iDirection);
	vec4 texColor = texture(cubemaps[nonuniformEXT(cubemapSlot)], dir);
	
	oColor = vec4(texColor.rgb, 1.0);
}