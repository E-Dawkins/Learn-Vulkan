#pragma once
#include "utils/singleton.h"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <array>
#include <optional>
#include <vector>

#include "renderer/mesh.h"
#include "renderer/shader.h"

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool IsComplete() const {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities = {};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct UniformBufferObject
{
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

struct MultisampleState
{
	VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
	VkBool32 sampleShadingEnabled = VK_FALSE;
	float minSampleShading = 1.f;
};

class App : public ISingleton<App>
{
private:
	GLFWwindow* mWindow = nullptr;

	VkInstance mInstance;
	VkDebugUtilsMessengerEXT mDebugMessenger;
	VkSurfaceKHR mWindowSurface;
	VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
	VkDevice mLogicalDevice;
	VkQueue mGraphicsQueue;
	VkQueue mPresentQueue;
	VkSwapchainKHR mSwapChain;
	std::vector<VkImage> mSwapChainImages;
	VkFormat mSwapChainImageFormat;
	VkExtent2D mSwapChainExtent;
	std::vector<VkImageView> mSwapChainImageViews;
	std::vector<VkFramebuffer> mSwapChainFramebuffers;
	std::vector<VkRenderPass> mRenderPasses;
	VkCommandPool mCommandPool;
	std::vector<Vertex> mVertices;
	std::vector<uint32_t> mIndices;
	VkBuffer mVertexBuffer;
	VkDeviceMemory mVertexBufferMemory;
	VkBuffer mIndexBuffer;
	VkDeviceMemory mIndexBufferMemory;
	std::vector<VkBuffer> mUniformBuffers;
	std::vector<VkDeviceMemory> mUniformBuffersMemory;
	std::vector<void*> mUniformBuffersMapped;
	VkDescriptorPool mDescriptorPool;
	std::vector<VkDescriptorSet> mDescriptorSets;
	std::vector<VkCommandBuffer> mCommandBuffers;
	std::vector<VkSemaphore> mImageAvailableSemaphores; // signal that an image has been acquired
	std::vector<VkSemaphore> mRenderFinishedSemaphores; // signal that rendering has finished
	std::vector<VkFence> mInFlightFences; // make sure only one frame is rendering at a time
	uint32_t mCurrentFrame = 0;
	uint32_t mMipLevels;
	VkImage mTextureImage;
	VkDeviceMemory mTextureImageMemory;
	VkImageView mTextureImageView;
	VkSampler mTextureSampler;
	VkImage mDepthImage;
	VkDeviceMemory mDepthImageMemory;
	VkImageView mDepthImageView;

	MultisampleState mMsaaState;

	VkImage mColorImage;
	VkDeviceMemory mColorImageMemory;
	VkImageView mColorImageView;

	Shader* mTempShader;

public:
	bool framebufferResized = false;

public:
	void Run();

	const VkDevice& GetLogicalDevice() const { return mLogicalDevice; }
	const MultisampleState& GetMsaaState() const { return mMsaaState; }
	const VkRenderPass& GetRenderPass(size_t _index) const {
		assert(_index < mRenderPasses.size());
		return mRenderPasses[_index];
	}

private:
	void InitWindow();

	void InitVulkan();
	void CreateInstance();
	std::vector<const char*> GetRequiredExtensions();
	void PrintUnsupportedExtensions(std::vector<const char*>& _extensions);
	bool CheckValidationLayerSupport();
	void SetupDebugMessenger();
	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& _createInfo);
	void CreateWindowSurface();
	VkSampleCountFlagBits GetMaxUsableSampleCount() const;
	void PickPhysicalDevice();
	bool IsDeviceSuitable(VkPhysicalDevice _device);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice _device);
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice _device) const;
	void CreateLogicalDevice();
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice _device) const;
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& _availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& _availablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& _capabilities);
	void CreateSwapChain();
	VkImageView CreateImageView(VkImage _image, VkFormat _format, VkImageAspectFlags _aspectFlags, uint32_t _mipLevels) const;
	void CreateImageViews();
	void CreateRenderPass();
	void CreateGraphicsPipeline();
	VkShaderModule CreateShaderModule(const std::vector<char>& _code) const;
	void CreateFramebuffers();
	void CreateCommandPool();
	void CreateBuffer(VkDeviceSize _size, VkBufferUsageFlags _usage, VkMemoryPropertyFlags _properties, VkBuffer& _buffer, VkDeviceMemory& _bufferMemory) const;
	void CopyBuffer(VkBuffer _srcBuffer, VkBuffer _dstBuffer, VkDeviceSize _size) const;
	void CreateImage(uint32_t _width, uint32_t _height, uint32_t _mipLevels, VkSampleCountFlagBits _numSamples, VkFormat _format, VkImageTiling _tiling, VkImageUsageFlags _usage, VkMemoryPropertyFlags _properties, VkImage& _image, VkDeviceMemory& _imageMemory) const;
	void TransitionImageLayout(VkImage _image, VkFormat _format, VkImageLayout _oldLayout, VkImageLayout _newLayout, uint32_t _mipLevels);
	void CopyBufferToImage(VkBuffer _buffer, VkImage _image, uint32_t _width, uint32_t _height);
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& _candidates, VkImageTiling _tiling, VkFormatFeatureFlags _features) const;
	void CreateColorResources();
	VkFormat FindDepthFormat();
	bool HasStencilComponent(VkFormat _format);
	void CreateDepthResources();
	void GenerateMipMaps(VkImage _image, VkFormat _imageFormat, int32_t _texWidth, int32_t _texHeight, uint32_t _mipLevels);
	void CreateTextureImage();
	void CreateTextureImageView();
	void CreateTextureSampler();
	void LoadModel();
	void CreateVertexBuffer();
	void CreateIndexBuffer();
	void CreateUniformBuffers();
	void CreateDescriptorPool();
	void CreateDescriptorSets();
	uint32_t FindMemoryType(uint32_t _typeFilter, VkMemoryPropertyFlags _properties) const;
	void CreateCommandBuffers();
	VkCommandBuffer BeginSingleTimeCommands() const;
	void EndSingleTimeCommands(VkCommandBuffer _commandBuffer) const;
	void RecordCommandBuffer(VkCommandBuffer _commandBuffer, uint32_t _imageIndex) const;
	void CreateSyncObjects();
	void RecreateSwapChain();
	void CleanupSwapChain();

	void MainLoop();
	void DrawFrame();
	void UpdateUniformBuffer(uint32_t _currentImage);
	void Cleanup();
};
