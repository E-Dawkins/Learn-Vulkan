#version 450

layout (binding = 1) uniform sampler2D texSampler;

layout (location = 0) in vec3 iColor;
layout (location = 1) in vec2 iTexCoord;

layout (location = 0) out vec4 oColor;

void main() {
	oColor = vec4(iColor * texture(texSampler, iTexCoord).rgb, 1.0);
}