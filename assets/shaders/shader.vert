#version 450
#include <vert_bindings.glsl>

layout (location = 0) out vec3 oColor;
layout (location = 1) out vec2 oTexCoord;

void main() {
	mat4 mvp = ubo.proj * ubo.view * getModelMatrix(gl_InstanceIndex);
	gl_Position = mvp * vec4(iPosition, 1.0);
	
	oColor = iColor;
	oTexCoord = iTexCoord;
}