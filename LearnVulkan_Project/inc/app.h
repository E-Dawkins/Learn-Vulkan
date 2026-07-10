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
#include "renderer/renderer_types.h"
#include "renderer/swapchain.h"
#include "renderer/texture.h"
#include "renderer/window.h"
#include "utils/buffer_utils.h"
#include "utils/input_manager.h"
#include "utils/type_defs.h"

class Material;
class Mesh;

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

	VkDescriptorPool mMaterialDescriptorPool;
	MaterialData mMaterialData;

	std::vector<FrameData> mFrameData;
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

	void SetupInput();
	void Event_MoveForward(float _scale);
	void Event_MoveRight(float _scale);
	void Event_MoveUp(float _scale);
	void Event_LockCursor(float _scale);
	void Event_LookAtOrigin();
	void Event_MouseMove(const glm::vec2& _scale);
	void Event_MouseScroll(const glm::vec2& _scale);
	void Event_IterateTextures();
	void Event_IterateColors();
	void Event_ToggleTextureLoad();

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

	void CreateMaterialDescriptorPool();

	void RecordCommandBuffer(VkCommandBuffer _commandBuffer, uint32_t _imageIndex) const;
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
