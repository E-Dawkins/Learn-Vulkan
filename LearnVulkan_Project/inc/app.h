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

class Mesh;
class Shader;
class Texture;

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

	VkSampler mTextureSampler;

	VkImage mDepthImage;
	VkDeviceMemory mDepthImageMemory;
	VkImageView mDepthImageView;

	MultisampleState mMsaaState;

	VkImage mColorImage;
	VkDeviceMemory mColorImageMemory;
	VkImageView mColorImageView;

	Shader* mTempShader;
	Mesh* mTempMesh;
	Texture* mTempTexture;

public:
	bool framebufferResized = false;

public:
	void Run();

	const VkPhysicalDevice& GetPhysicalDevice() const { return mPhysicalDevice; }
	const VkDevice& GetLogicalDevice() const { return mLogicalDevice; }
	const MultisampleState& GetMsaaState() const { return mMsaaState; }
	const VkRenderPass& GetRenderPass(size_t _index) const {
		assert(_index < mRenderPasses.size());
		return mRenderPasses[_index];
	}

	VkCommandBuffer BeginSingleTimeCommands() const;
	void EndSingleTimeCommands(VkCommandBuffer _commandBuffer) const;

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
	void CreateImageViews();
	void CreateRenderPass();
	void CreateFramebuffers();
	void CreateCommandPool();
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& _candidates, VkImageTiling _tiling, VkFormatFeatureFlags _features) const;
	void CreateColorResources();
	VkFormat FindDepthFormat();
	bool HasStencilComponent(VkFormat _format);
	void CreateDepthResources();
	void CreateTextureSampler();
	void CreateUniformBuffers();
	void CreateDescriptorPool();
	void CreateDescriptorSets();
	uint32_t FindMemoryType(uint32_t _typeFilter, VkMemoryPropertyFlags _properties) const;
	void CreateCommandBuffers();
	void RecordCommandBuffer(VkCommandBuffer _commandBuffer, uint32_t _imageIndex) const;
	void CreateSyncObjects();
	void RecreateSwapChain();
	void CleanupSwapChain();

	void MainLoop();
	void DrawFrame();
	void UpdateUniformBuffer(uint32_t _currentImage);
	void Cleanup();
};
