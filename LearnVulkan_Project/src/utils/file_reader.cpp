#include "pch.h"

#include "utils/file_reader.h"

void IFileReader::EnforceFileExtension(const std::filesystem::path& _filePath, const std::string& _extension) {
	if (_filePath.extension() != _extension) {
		throw std::runtime_error(std::format("Expected *{} extension for file \"{}\"!\n", _extension, _filePath.filename().string()));
	}
}

void IFileReader::ReadWholeFile(const std::filesystem::path& _filepath, std::ios_base::openmode _mode, std::vector<char>& _outBuffer) {
	std::ifstream file(_filepath, _mode);

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file! File path: " + _filepath.string());
	}

	// Since we started reading from the EOF, it is trivial to get the file size
	size_t fileSize = (size_t)(file.tellg());
	_outBuffer.resize(fileSize);

	// Return back to start of file, and read the entire thing at once
	file.seekg(0);
	file.read(_outBuffer.data(), fileSize);

	file.close();
}

void IFileReader::ReadFileAsAil(const std::filesystem::path& _filepath, AilReader& _outReader) {
	std::ifstream file(_filepath);

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file! File path: " + _filepath.string());
	}

	_outReader.Reset();
	_outReader.Parse(file);
}

AilNode::AilNode(const std::string& _name, const std::string& _value) 
	: mName(_name), mValue(_value) {}

AilNode::~AilNode() {
	for (AilNode* n : mSubnodes) {
		delete n;
	}
}

void AilNode::AddSubnode(AilNode* _node) {
	if (_node) {
		mSubnodes.push_back(_node);
	}
}

AilNode* AilNode::GetSubnode(size_t _index) const {
	if (_index < mSubnodes.size()) {
		return mSubnodes[_index];
	}
	
	return nullptr;
}

AilNode* AilNode::GetSubnode(const std::string& _name) const {
	for (AilNode* sn : mSubnodes) {
		if (sn->mName == _name) {
			return sn;
		}
	}

	return nullptr;
}

glm::vec4 AilNode::ValAsVec4() const {
	glm::vec4 out = {};

	for (glm::length_t i = 0; i < std::min(mSubnodes.size(), 4ull); i++) {
		out[i] = std::stof(mSubnodes[i]->mValue);
	}

	return out;
}

std::string AilNode::GetAsStr(bool _trimQuotations) const {
	size_t start = 0, count = mValue.size();

	if (_trimQuotations) {
		if (mValue.front() == '"') {
			start++;
			count--;
		}
		if (mValue.back() == '"') count--;
	}

	return mValue.substr(start, count);
}

AilReader::~AilReader() {
	Reset();
}

void AilReader::Reset() {
	delete mRootNode;
	mRootNode = nullptr;

	while (!mNodeStack.empty()) {
		mNodeStack.pop();
	}
}

void AilReader::Parse(std::ifstream& _file) {
	mRootNode = new AilNode("root");
	mNodeStack.push(mRootNode);

	for (std::string line; std::getline(_file, line);) {
		Trim(line);

		// Skip empty lines
		if (line.empty()) {
			continue;
		}

		// End of 'type'
		if (line == "}") {
			mNodeStack.pop();
			continue;
		}

		if (line.starts_with("type:")) {
			size_t bracePos = line.find("{");
			line = line.substr(5, bracePos - 5);

			AilNode* typeNode = new AilNode(line);

			mNodeStack.top()->AddSubnode(typeNode);
			mNodeStack.push(typeNode);
		}
		else {
			size_t initialPos = line.find(',');

			// Split by ','
			if (initialPos != std::string::npos) {
				AilNode* elemNode = new AilNode("");

				mNodeStack.top()->AddSubnode(elemNode);

				for (size_t pos = initialPos; pos != std::string::npos; pos = line.find(',')) {
					AilNode* dataNode = new AilNode("", line.substr(0, pos));

					elemNode->AddSubnode(dataNode);

					line.erase(0, pos + 1); // +1 for delimiter
				}

				// Final element
				if (!line.empty()) {
					AilNode* dataNode = new AilNode("", line);

					elemNode->AddSubnode(dataNode);
				}
			}
			else {
				initialPos = line.find('=');

				AilNode* dataNode = nullptr;

				if (initialPos != std::string::npos) {
					dataNode = new AilNode(line.substr(0, initialPos), line.substr(initialPos + 1));
				}
				else {
					dataNode = new AilNode("", line);
				}

				mNodeStack.top()->AddSubnode(dataNode);
			}
		}
	}
}

void AilReader::PrintNode(AilNode* _node, size_t _indent) {
	if (!_node) {
		return;
	}

	if (_indent > 0) {
		for (size_t i = 0; i < _indent - 1; i++) {
			std::cout << '\t';
		}

		std::string name = _node->GetRawName(), value = _node->GetRawValue();
		if (name.empty()) name = "''";
		if (value.empty()) value = "''";

		std::cout << "" << name << "\t";
		std::cout << "" << value << "\n";
	}

	for (AilNode* sn : *_node) {
		PrintNode(sn, _indent + 1);
	}
}

AilNode* AilReader::GetNode(std::string _nodePath) const {
	AilNode* current = mRootNode;

	auto find_by_index = [&](const std::string& _nameToFind) {
		if (std::all_of(_nameToFind.begin(), _nameToFind.end(), ::isdigit)) {
			size_t index = std::stoull(_nameToFind);

			if (index < current->size()) {
				current = *current->begin();
				return true;
			}
		}

		return false;
	};

	auto find_by_name = [&](const std::string& _nameToFind) {
		for (AilNode* sn : *current) {
			if (sn->GetRawName() == _nameToFind) {
				current = sn;
				return true;
			}
		}

		return false;
	};

	for (size_t pipePos = _nodePath.find('|'); pipePos != std::string::npos; pipePos = _nodePath.find('|')) {
		std::string nameToFind = _nodePath.substr(0, pipePos);
		_nodePath.erase(0, pipePos + 1);

		if (!find_by_index(nameToFind)) {
			if (!find_by_name(nameToFind)) {
				return nullptr;
			}
		}
	}

	// Final path element
	if (!find_by_index(_nodePath)) {
		if (!find_by_name(_nodePath)) {
			return nullptr;
		}
	}

	return current;
}

// Following trim functions taken from here:
// https://stackoverflow.com/questions/216823/how-can-i-trim-a-stdstring
void AilReader::LTrim(std::string& _str) {
	_str.erase(_str.begin(), std::find_if(_str.begin(), _str.end(), [](unsigned char _char) {
		return !std::isspace(_char);
	}));
}

void AilReader::RTrim(std::string& _str) {
	_str.erase(std::find_if(_str.rbegin(), _str.rend(), [](unsigned char _char) {
		return !std::isspace(_char);
	}).base(), _str.end());
}

void AilReader::Trim(std::string& _str) {
	RTrim(_str);
	LTrim(_str);
}
