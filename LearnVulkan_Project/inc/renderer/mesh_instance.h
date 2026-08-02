#pragma once
#include <vulkan/vulkan.h>

#include "math/transform.h"
#include "renderer/renderer_types.h"

class MeshAsset;
class Material;

class MeshInstance
{
	friend RenderBucketMap;

protected:
	std::weak_ptr<MeshAsset> mRefMesh;
	std::weak_ptr<Material> mMaterial;

	// Any transform in this container will be rendered as a mesh instance.
	std::vector<RenderTransform> mTransforms;

	// This is bound during draw to map instance index to transform index.
	std::vector<uint32_t> mTransformIndices;

	RenderBucketMap::RuntimeRenderId mRuntimeId;

public:
	MeshInstance(std::weak_ptr<MeshAsset> _mesh);
	~MeshInstance() {}

	void BindMeshResources(const VkCommandBuffer& _commandBuffer);
	void DrawMesh(const VkCommandBuffer& _commandBuffer) const;

	void AddInstance(const RenderTransform& _transform);

	inline void SetMaterial(std::weak_ptr<Material> _material) { mMaterial = _material; }
	
	inline size_t GetInstanceCount() const { return mTransforms.size(); }
	inline RenderTransform& GetTransform(size_t _index) {
		assert(_index < mTransforms.size());
		return mTransforms[_index];
	}
	inline const RenderBucketMap::RuntimeRenderId& GetRuntimeId() { return mRuntimeId; }
};
