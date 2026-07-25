#include "pch.h"
#include "renderer/mesh_instance.h"

#include "app.h"
#include "renderer/material.h"
#include "renderer/mesh_asset.h"
#include "renderer/shader.h"

MeshInstance::MeshInstance(std::weak_ptr<MeshAsset> _mesh)
	: mRefMesh(_mesh) {}

void MeshInstance::BindMeshResources(const VkCommandBuffer& _commandBuffer) {
	auto lockedMaterial = mMaterial.lock();
	auto lockedMesh = mRefMesh.lock();

	if (!lockedMaterial || !lockedMesh) {
		return;
	}

	lockedMaterial->BindMaterialResources(_commandBuffer);

	// Bind buffers
	static const VkDeviceSize zeroOffset = 0;

	vkCmdBindVertexBuffers(_commandBuffer, 0, 1, &lockedMesh->mVertexBuffer, &zeroOffset);
	vkCmdBindIndexBuffer(_commandBuffer, lockedMesh->mIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

	// Bind current transform data
	MeshData& meshData = App::GetInstance().GetMeshData();
	meshData.BindTransformIndices(mTransformIndices);

	for (size_t i = 0; i < mTransforms.size(); i++) {
		mTransforms[i].UpdateMappedMatrix();
	}
}

void MeshInstance::DrawMesh(const VkCommandBuffer& _commandBuffer) const {
	auto lockedMesh = mRefMesh.lock();
	if (!lockedMesh) {
		return;
	}

	// Draw the vertices in the vertex buffer, using the index buffer
	vkCmdDrawIndexed(
		_commandBuffer, 
		static_cast<uint32_t>(lockedMesh->mIndices.size()),
		static_cast<uint32_t>(mTransforms.size()), // instance count
		0,
		0, 
		0
	);
}

void MeshInstance::AddInstance(const RenderTransform& _transform) {
	mTransforms.push_back(_transform);
	mTransformIndices.push_back({});

	MeshData& meshData = App::GetInstance().GetMeshData();
	meshData.MapFreeTransform(mTransforms.back(), mTransformIndices.back());
}
