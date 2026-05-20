#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (set = 1, binding = 0) uniform sampler2D textures[];
layout (set = 1, binding = 1) readonly buffer RuntimeToTexId {
	uint texSlot[];
};

layout(push_constant) uniform PushConstants {
	uint runtimeId;
} pushConsants;

layout (location = 0) in vec3 iColor;
layout (location = 1) in vec2 iTexCoord;

layout (location = 0) out vec4 oColor;

void main() {
	uint slot = texSlot[pushConsants.runtimeId];
	vec4 texColor = texture(textures[nonuniformEXT(slot)], iTexCoord);

	oColor = vec4(iColor * texColor.rgb, 1.0);
}