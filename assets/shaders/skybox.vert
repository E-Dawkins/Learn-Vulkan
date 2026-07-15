#version 450
#include <vert_bindings.glsl>

layout (location = 0) out vec3 oDirection;

void main() {
	mat4 viewNoTranslation = mat4(mat3(ubo.view));
	gl_Position = ubo.proj * viewNoTranslation * vec4(iPosition, 1.0);
	
	// We swizzle the position so that the cubemap
	// direction works with our set world axes
	oDirection = iPosition.xzy;
}