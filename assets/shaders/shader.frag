#version 450

layout (set = 1, binding = 0) uniform sampler2D textures[];

layout (location = 0) in vec3 iColor;
layout (location = 1) in vec2 iTexCoord;

layout (location = 0) out vec4 oColor;

void main() {
	oColor = vec4(iColor * texture(textures[0], iTexCoord).rgb, 1.0);
}