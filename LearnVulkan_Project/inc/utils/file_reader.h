#pragma once
#include <filesystem>
#include <stack>

class AilReader;

class IFileReader
{
protected:
	void EnforceFileExtension(const std::filesystem::path& _filePath, const std::string& _extension);

	void ReadWholeFile(const std::filesystem::path& _filepath, std::ios_base::openmode _mode, std::vector<char>& _outBuffer);

	void ReadFileAsAil(const std::filesystem::path& _filepath, AilReader& _outReader);
};

struct AilNode
{
	std::string name;
	std::string value;

	std::vector<AilNode*> subnodes;

	~AilNode() {
		for (AilNode* n : subnodes) {
			delete n;
		}
	}
};

class AilReader
{
private:
	AilNode* mRootNode = nullptr;
	std::stack<AilNode*> mNodeStack;

public:
	~AilReader();

	void Reset();
	void Parse(std::ifstream& _file);
	void PrintNode(AilNode* _node, size_t _indent = 0);

	AilNode* GetNode(std::string _nodePath) const;
	std::string GetAsStr(const std::string& _nodePath, bool _trimQuotations = true) const;
	size_t GetAsInt(const std::string& _nodePath) const;
	float GetAsFloat(const std::string& _nodePath) const;
	glm::vec4 GetAsVec4(const std::string& _nodePath) const;
	glm::vec3 GetAsVec3(const std::string& _nodePath) const;
	glm::vec2 GetAsVec2(const std::string& _nodePath) const;

	template<typename T>
	T GetAsIntCasted(const std::string& _nodePath) const {
		return static_cast<T>(GetAsInt(_nodePath));
	}

private:
	void LTrim(std::string& _str);
	void RTrim(std::string& _str);
	void Trim(std::string& _str);
};