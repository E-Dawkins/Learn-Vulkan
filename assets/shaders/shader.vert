#version 450
#include <vert_bindings.glsl>

layout (location = 0) out vec3 oColor;
layout (location = 1) out vec2 oTexCoord;

void main() {
	gl_Position = ubo.proj * ubo.view * pc.model * vec4(iPosition, 1.0);
	
	oColor = iColor;
	oTexCoord = iTexCoord;
}