#version 450
#extension GL_EXT_nonuniform_qualifier : require
#include <frag_bindings.glsl>

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