#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (set = 1, binding = 0) uniform sampler2D textures[];
layout (set = 1, binding = 1) readonly buffer DenseIdToTextureSlot {
	uint texSlots[];
};

struct MaterialParams
{
	uint denseTexIds[8];
	vec4 colorVars[8];
	int intVars[8];
};

layout (set = 1, binding = 2) readonly buffer MaterialParamsBuffer {
	MaterialParams matParams[];
};

layout (set = 1, binding = 3) readonly buffer DenseIdToMaterialSlot {
	uint matSlots[];
};

layout(push_constant) uniform PushConstants {
	uint denseMatSlotId;
} pc;

layout (location = 0) in vec3 iColor;
layout (location = 1) in vec2 iTexCoord;

layout (location = 0) out vec4 oColor;

void main() {
	uint matSlot = matSlots[pc.denseMatSlotId];
	MaterialParams params = matParams[matSlot];

	uint texSlot = texSlots[params.denseTexIds[0]];
	vec4 texColor = texture(textures[nonuniformEXT(texSlot)], iTexCoord);

	int colorIndex = params.intVars[0];
	vec4 selectedColor = params.colorVars[colorIndex];

	oColor = vec4(iColor * texColor.rgb, 1.0) * selectedColor;
}