#pragma once
#include <vulkan/vulkan.h>
#include "renderer/texture.h"

struct SwapchainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities = {};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class Swapchain
{
private:
	VkSwapchainKHR mSwapchain;

	VkFormat mFormat;
	VkExtent2D mExtent;

	uint32_t mImageCount = 0;
	std::vector<VkImage> mImages;
	std::vector<VkImageView> mImageViews;

	std::vector<VkFramebuffer> mFramebuffers;

	std::vector<VkSemaphore> mRenderFinishedSemaphores; // signal that rendering has finished for an image

public:
	void CreateSwapchain();
	void CreateImageViews();
	void CreateFramebuffers(VkImageView _colorView, VkImageView _depthView);
	void CreateSyncObjects();

	void CleanupSwapchain();
	void CleanupSyncObjects();

	SwapchainSupportDetails QuerySwapChainSupport(VkPhysicalDevice _device, VkSurfaceKHR _surface) const;
	VkResult AcquireNextImage(VkSemaphore _imageAvailableSemaphore, uint32_t& _outIndex) const;

	inline const VkSwapchainKHR& GetSwapchain() const { return mSwapchain; }
	inline const VkFormat& GetFormat() const { return mFormat; }
	inline const VkExtent2D& GetExtent() const { return mExtent; }
	inline uint32_t GetImageCount() const { return mImageCount; }
	inline const VkFramebuffer& GetFramebuffer(size_t _index) const {
		assert(_index < mFramebuffers.size());
		return mFramebuffers[_index];
	}
	inline const VkSemaphore& GetRenderFinishedSemaphore(size_t _index) const {
		assert(_index < mRenderFinishedSemaphores.size());
		return mRenderFinishedSemaphores[_index];
	}

private:
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& _availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& _availablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& _capabilities);
};