#version 450
#extension GL_EXT_nonuniform_qualifier : require
#include <frag_bindings.glsl>

layout (location = 0) in vec3 iDirection;

layout (location = 0) out vec4 oColor;

void main() {
	uint matSlot = matSlots[pc.denseMatSlotId];
	MaterialParams params = matParams[matSlot];

	uint index = params.intVars[0];

	if (index == 2) {
		oColor = params.colorVars[0];
	}
	else {
		uint cubemapSlot = cubemapSlots[params.denseCubemapIds[index]];
		vec4 texColor = texture(cubemaps[nonuniformEXT(cubemapSlot)], normalize(iDirection));
		
		oColor = vec4(texColor.rgb, 1.0);
	}
}