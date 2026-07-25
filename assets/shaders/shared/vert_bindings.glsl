/* ----- Layout Bindings ----- */
layout (set = 0, binding = 0) uniform CameraData {
	mat4 view;
	mat4 proj;
} ubo;

layout (set = 2, binding = 0) readonly buffer InstanceTransforms {
	mat4 transforms[];
};

layout (set = 2, binding = 1) readonly buffer InstToTransformIndex {
	uint transformIndices[];
};

layout (location = 0) in vec3 iPosition;
layout (location = 1) in vec3 iColor;
layout (location = 2) in vec2 iTexCoord;


/* ----- Helper Functions ----- */
mat4 getModelMatrix(uint _instIndex) {
	return transforms[transformIndices[_instIndex]];
}