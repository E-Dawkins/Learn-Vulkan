#pragma once
#include "interfaces/singleton.h"

#include <vulkan/vulkan.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <array>
#include <optional>
#include <vector>

#include "renderer/camera.h"
#include "renderer/swapchain.h"
#include "renderer/texture.h"
#include "renderer/window.h"
#include "utils/buffer_utils.h"
#include "utils/type_defs.h"

class Material;
class Mesh;

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool IsComplete() const {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
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
	std::unique_ptr<Window> mWindow;
	std::unique_ptr<Swapchain> mSwapchain;

	VkInstance mInstance;
	VkDebugUtilsMessengerEXT mDebugMessenger;
	VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
	VkDevice mLogicalDevice;
	VkQueue mGraphicsQueue;
	VkQueue mPresentQueue;
	std::vector<VkRenderPass> mRenderPasses;
	VkCommandPool mCommandPool;

	std::vector<VkBuffer> mUniformBuffers;
	std::vector<VkDeviceMemory> mUniformBuffersMemory;
	std::vector<void*> mUniformBuffersMapped;

	VkDescriptorPool mFrameDescriptorPool;
	std::vector<VkDescriptorSet> mFrameDescriptorSets;

	VkDescriptorPool mMaterialDescriptorPool;
	VkDescriptorSet mMaterialDescriptorSet;
	std::unordered_map<AssetDefs::TextureSlot, VkSampler> mTextureSlotToSampler;
	Utils::BufferUtils::Ssbo mDenseIdToTextureSlot;
	Utils::BufferUtils::Ssbo mMaterialParamsBuffer;
	Utils::BufferUtils::Ssbo mDenseIdToMaterialSlot;

	std::vector<VkCommandBuffer> mCommandBuffers;
	std::vector<VkSemaphore> mImageAvailableSemaphores; // signal that an image has been acquired
	std::vector<VkFence> mInFlightFences; // make sure only one frame is rendering at a time
	uint32_t mCurrentFrame = 0;

	MultisampleState mMsaaState;
	RuntimeTexture mDepthTexture;
	RuntimeTexture mColorTexture;

	std::weak_ptr<Mesh> mTempMesh;
	std::unique_ptr<FlyCamera> mCamera;

	bool mFramebufferResized = false;

public:
	void Run();

	inline const VkInstance& GetVulkanInstance() const { return mInstance; }
	inline const VkPhysicalDevice& GetPhysicalDevice() const { return mPhysicalDevice; }
	inline const VkDevice& GetLogicalDevice() const { return mLogicalDevice; }
	inline const MultisampleState& GetMsaaState() const { return mMsaaState; }
	inline const VkRenderPass& GetRenderPass(size_t _index) const {
		assert(_index < mRenderPasses.size());
		return mRenderPasses[_index];
	}
	inline const Window& GetWindow() const { return *mWindow; }

	VkCommandBuffer BeginSingleTimeCommands() const;
	void EndSingleTimeCommands(VkCommandBuffer _commandBuffer) const;

	// Calls 'FindQueueFamilies' with current physical device
	inline QueueFamilyIndices FindCurrentQueueFamilies() const { return FindQueueFamilies(mPhysicalDevice); }

private:
	void InitWindow();

	void ProcessMouseMovement(const glm::vec2& _deltaPos);
	void ProcessMouseScroll(const glm::vec2& _scrollDelta);
	void ProcessMouseInput(int _button, int _action);
	void ProcessKeyInput(int _key, int _action);

	void InitVulkan();
	void CreateInstance();
	std::vector<const char*> GetRequiredExtensions();
	void PrintUnsupportedExtensions(std::vector<const char*>& _extensions);
	bool CheckValidationLayerSupport();
	void SetupDebugMessenger();
	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& _createInfo);
	VkSampleCountFlagBits GetMaxUsableSampleCount() const;
	void PickPhysicalDevice();
	bool IsDeviceSuitable(VkPhysicalDevice _device);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice _device);
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice _device) const;
	void CreateLogicalDevice();
	void CreateRenderPass();
	void CreateCommandPool();
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& _candidates, VkImageTiling _tiling, VkFormatFeatureFlags _features) const;
	void CreateColorResources();
	VkFormat FindDepthFormat();
	void CreateDepthResources();

	void CreateUniformBuffers();

	void CreateDescriptorPools();

	void CreateFrameDescriptorSets();

	void CreateMaterialBuffers();
	void CreateMaterialDescriptorSet();
	void OnTextureLoaded(std::weak_ptr<Texture> _tex, AssetDefs::DenseId _denseId, AssetDefs::TextureSlot _texSlot);
	void OnTextureUnloaded(AssetDefs::DenseId _denseId);
	void CreateTextureSamplerForSlot(AssetDefs::TextureSlot _texSlot);
	void OnMaterialLoaded(std::weak_ptr<Material> _mat, AssetDefs::DenseId _denseId, AssetDefs::MaterialSlot _matSlot);
	void OnMaterialUnloaded(AssetDefs::DenseId _denseId);

	uint32_t FindMemoryType(uint32_t _typeFilter, VkMemoryPropertyFlags _properties) const;
	void CreateCommandBuffers();
	void RecordCommandBuffer(VkCommandBuffer _commandBuffer, uint32_t _imageIndex) const;
	void CreateSyncObjects();
	void RecreateSwapchain();
	void CleanupSwapchain(bool _isFinalCleanup = false);

	void Start();
	void MainLoop();
	void DrawFrame(float _deltaTime);
	void UpdateUniformBuffer(uint32_t _currentImage, float _deltaTime);

private:
	void OnInitialized() override;
	void OnCleanup() override;
};
