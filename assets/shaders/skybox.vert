#version 450

layout (set = 0, binding = 0) uniform CameraData {
	mat4 view;
	mat4 proj;
} ubo;

layout (location = 0) in vec3 iPosition;
layout (location = 1) in vec3 iColor;
layout (location = 2) in vec2 iTexCoord;

layout(push_constant) uniform PushConstants {
	layout(offset = 0) mat4 model;
} pc;

layout (location = 0) out vec3 oDirection;

void main() {
	mat4 viewNoTranslation = mat4(mat3(ubo.view));
	gl_Position = ubo.proj * viewNoTranslation * pc.model * vec4(iPosition, 1.0);

	vec3 worldDir = iPosition;
	vec3 cubeDir = vec3(
		worldDir.x,
		worldDir.z,
		worldDir.y
	);

	oDirection = cubeDir;
}