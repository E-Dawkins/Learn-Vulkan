#include "pch.h"
#include "renderer/mesh.h"

#include "app.h"
#include "renderer/material.h"
#include "utils/buffer_utils.h"

VkVertexInputBindingDescription Vertex::GetBindingDescription() {
	static VkVertexInputBindingDescription bindingDescription{
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};

	return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::GetAttributeDescriptions() {
	static std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{
		// Position attribute
		VkVertexInputAttributeDescription{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, pos)
		},

		// Color attribute
		VkVertexInputAttributeDescription{
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, color)
		},

		// Texcoord attribute
		VkVertexInputAttributeDescription{
			.location = 2,
			.binding = 0,
			.format = VK_FORMAT_R32G32_SFLOAT,
			.offset = offsetof(Vertex, texCoord)
		}
	};

	return attributeDescriptions;
}

bool Vertex::operator == (const Vertex& _other) const {
	return pos == _other.pos && color == _other.color && texCoord == _other.texCoord;
}

Mesh::Mesh(const std::filesystem::path& _filePath) {
	EnforceFileExtension(_filePath, ".mesh");

	AilReader reader;
	ReadFileAsAil(_filePath, reader);

	auto pathNode = reader.TryGetNode("path");
	if (!pathNode) {
		throw std::runtime_error("Could not find AilNode: 'path' for Mesh [" + _filePath.string() + "]\n");
	}

	LoadFromFile(pathNode->GetAsStr());
	CreateVertexBuffer();
	CreateIndexBuffer();
}

Mesh::~Mesh() {
	const VkDevice& logicalDevice = App::GetInstance().GetLogicalDevice();

	vkDestroyBuffer(logicalDevice, mIndexBuffer, nullptr);
	vkFreeMemory(logicalDevice, mIndexBufferMemory, nullptr);

	vkDestroyBuffer(logicalDevice, mVertexBuffer, nullptr);
	vkFreeMemory(logicalDevice, mVertexBufferMemory, nullptr);
}

void Mesh::SetMaterial(Material* _material) {
	assert(_material);

	mMaterial = _material;
}

void Mesh::BindMeshResources(const VkCommandBuffer& _commandBuffer) const {
	mMaterial->BindMaterialResources(_commandBuffer);

	// Bind buffers
	static const VkDeviceSize zeroOffset = 0;

	vkCmdBindVertexBuffers(_commandBuffer, 0, 1, &mVertexBuffer, &zeroOffset);
	vkCmdBindIndexBuffer(_commandBuffer, mIndexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::DrawMesh(const VkCommandBuffer& _commandBuffer) const {
	// Draw the vertices in the vertex buffer, using the index buffer
	vkCmdDrawIndexed(_commandBuffer, static_cast<uint32_t>(mIndices.size()), 1, 0, 0, 0);
}

void Mesh::LoadFromFile(const std::filesystem::path& _filePath) {
	EnforceFileExtension(_filePath, ".obj");

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, _filePath.string().c_str())) {
		throw std::runtime_error(err);
	}

	std::unordered_map<Vertex, uint32_t> uniqueVertices = {};

	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			Vertex vertex = {
				.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				},
				.color = { 1.f, 1.f, 1.f },
				.texCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.f - attrib.texcoords[2 * index.texcoord_index + 1]
				},
			};


			// Avoid duplicate vertices
			if (uniqueVertices.count(vertex) == 0) {
				uniqueVertices[vertex] = static_cast<uint32_t>(mVertices.size());
				mVertices.push_back(vertex);
			}

			mIndices.push_back(uniqueVertices[vertex]);
		}
	}
}

void Mesh::CreateVertexBuffer() {
	const VkDevice& logicalDevice = App::GetInstance().GetLogicalDevice();

	VkDeviceSize bufferSize = sizeof(mVertices[0]) * mVertices.size();

	VkBuffer stagingBuffer = {};
	VkDeviceMemory stagingBufferMemory = {};
	Utils::BufferUtils::CreateBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	// Copy the vertex data to the vertex data buffer
	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, mVertices.data(), (size_t)(bufferSize));
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	Utils::BufferUtils::CreateBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mVertexBuffer,
		mVertexBufferMemory
	);

	Utils::BufferUtils::CopyBuffer(stagingBuffer, mVertexBuffer, bufferSize);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
}

void Mesh::CreateIndexBuffer() {
	const VkDevice& logicalDevice = App::GetInstance().GetLogicalDevice();

	VkDeviceSize bufferSize = sizeof(mIndices[0]) * mIndices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	Utils::BufferUtils::CreateBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	// Copy the index data to the index data buffer
	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, mIndices.data(), (size_t)(bufferSize));
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	Utils::BufferUtils::CreateBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mIndexBuffer,
		mIndexBufferMemory
	);

	Utils::BufferUtils::CopyBuffer(stagingBuffer, mIndexBuffer, bufferSize);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
}
