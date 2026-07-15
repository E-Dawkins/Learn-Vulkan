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