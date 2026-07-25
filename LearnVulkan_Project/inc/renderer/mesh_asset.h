#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <vulkan/vulkan.h>

#include <array>

#include "interfaces/asset.h"
#include "interfaces/file_reader.h"
#include "utils/hash_utils.h"

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription GetBindingDescription();
	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();
	bool operator == (const Vertex& _other) const;
};

namespace std {
	template<> struct hash<Vertex>
	{
		size_t operator () (const Vertex& _vertex) const {
			size_t seed = 0;

			Utils::HashUtils::HashCombine(seed, hash<glm::vec3>()(_vertex.pos));
			Utils::HashUtils::HashCombine(seed, hash<glm::vec3>()(_vertex.color));
			Utils::HashUtils::HashCombine(seed, hash<glm::vec2>()(_vertex.texCoord));

			return seed;
		}
	};
}

class MeshAsset : IFileReader, public IAsset
{
	friend class MeshInstance;

private:
	std::vector<Vertex> mVertices;
	std::vector<uint32_t> mIndices;
	VkBuffer mVertexBuffer;
	VkDeviceMemory mVertexBufferMemory;
	VkBuffer mIndexBuffer;
	VkDeviceMemory mIndexBufferMemory;

public:
	MeshAsset(const std::filesystem::path& _filePath);
	~MeshAsset();

private:
	void LoadFromFile(const std::filesystem::path& _filePath);
	void CreateVertexBuffer();
	void CreateIndexBuffer();
};
