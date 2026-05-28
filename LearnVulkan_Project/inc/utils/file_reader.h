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

template <typename T>
concept ValidNodeValue = 
	   std::is_same_v<T, size_t>
	|| std::is_same_v<T, float>
	|| std::is_same_v<T, glm::vec4>
	|| std::is_same_v<T, glm::vec3>
	|| std::is_same_v<T, glm::vec2>;

struct AilNode
{
private:
	std::string mName;
	std::string mValue;

public:
	std::vector<AilNode> subnodes;

public:
	AilNode(const std::string& _name = "", const std::string& _value = "");

	inline const std::string& GetRawName() const { return mName; }
	inline const std::string& GetRawValue() const { return mValue; }

	bool TryGetSubnode(size_t _index, AilNode& _outNode) const;
	bool TryGetSubnode(const std::string& _name, AilNode& _outNode) const;

private:
	glm::vec4 ValAsVec4() const;

public:
	std::string GetAsStr(bool _trimQuotations = true) const;

	template<ValidNodeValue T>
	T Get() const {
		if constexpr (std::is_same_v<T, size_t>) {
			return std::stoull(mValue);
		}
		else if constexpr (std::is_same_v<T, float>) {
			return std::stof(mValue);
		}
		else if constexpr (std::is_same_v<T, glm::vec4>) {
			return ValAsVec4();
		}
		else if constexpr (std::is_same_v<T, glm::vec3>) {
			return glm::vec3(ValAsVec4());
		}
		else if constexpr (std::is_same_v<T, glm::vec2>) {
			return glm::vec2(ValAsVec4());
		}
	}

	template<typename T>
	T GetAsSizetCasted() const {
		return static_cast<T>(Get<size_t>());
	}
};

class AilReader
{
private:
	AilNode mRootNode;
	std::stack<AilNode*> mNodeStack;

public:
	~AilReader();

	void Reset();
	void Parse(std::ifstream& _file);
	void PrintNode(const AilNode& _node, size_t _indent = 0);

	bool TryGetNode(std::string _nodePath, AilNode& _outNode) const;

private:
	void LTrim(std::string& _str);
	void RTrim(std::string& _str);
	void Trim(std::string& _str);
};