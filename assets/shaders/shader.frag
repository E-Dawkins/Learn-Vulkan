#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (set = 1, binding = 0) uniform sampler2D textures[];
layout (set = 1, binding = 1) readonly buffer RuntimeToTexId {
	uint texSlot[];
};

layout(push_constant) uniform PushConstants {
	uint runtimeTexIds[8];
	vec4 colors[6];
} matParams;

layout (location = 0) in vec3 iColor;
layout (location = 1) in vec2 iTexCoord;

layout (location = 0) out vec4 oColor;

void main() {
	uint slot = texSlot[matParams.runtimeTexIds[0]];
	vec4 texColor = texture(textures[nonuniformEXT(slot)], iTexCoord);

	// We are using runtimeTexIds for a temporary int value (push constant range too small ;-;)
	vec4 selectedColor = matParams.colors[matParams.runtimeTexIds[1]];

	oColor = vec4(iColor * texColor.rgb, 1.0) * selectedColor;
}