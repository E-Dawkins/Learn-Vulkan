#include "pch.h"
#include "renderer/swapchain.h"

#include "app.h"
#include "utils/debug_logger.h"
#include "utils/image_utils.h"

constexpr bool gVsyncEnabled = true;

void Swapchain::CreateSwapchain() {
	LOG_MSG("Creating swapchain", LogVerbosity::Info);

	const App& appInst = App::GetInstance();
	const VkSurfaceKHR& surface = appInst.GetWindow().GetWindowSurface();
	const VkDevice& logicalDevice = appInst.GetLogicalDevice();

	SwapchainSupportDetails swapChainSupport = QuerySwapChainSupport(appInst.GetPhysicalDevice(), surface);

	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes, gVsyncEnabled);
	VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

	// This is the minimum number of images that are required to function,
	// however using this number may stall our program, so +1 is recommended
	mImageCount = swapChainSupport.capabilities.minImageCount + 1;

	// We need to clamp to the max image count, but also need
	// to check if '0' is the max, which specifies no maximum
	if (swapChainSupport.capabilities.maxImageCount > 0 && mImageCount > swapChainSupport.capabilities.maxImageCount) {
		mImageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = mImageCount,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1, // always 1, unless making a 3d app
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
	};

	QueueFamilyIndices indices = appInst.FindCurrentQueueFamilies();
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	// Check for ownership of images, sharing mode exclusive means
	// that ownership must be explicitly transferred between queues
	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; // optional
		createInfo.pQueueFamilyIndices = nullptr; // optional
	}

	// Specify any transform we should apply to swap chain images
	// i.e. 90 deg rotation or a horizontal flip
	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

	// Specify if the alpha channel should be used to blend with
	// other windows in the system, almost always set to opaque
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	// If .clipped is set to true then we ignore pixels that are
	// obscured by other windows, for performance we enable this
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &mSwapchain) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create swap chain!");
	}

	// Store swap chain image handles
	vkGetSwapchainImagesKHR(logicalDevice, mSwapchain, &mImageCount, nullptr);
	mImages.resize(mImageCount);
	vkGetSwapchainImagesKHR(logicalDevice, mSwapchain, &mImageCount, mImages.data());

	// And also store image format + extent for future use
	mFormat = surfaceFormat.format;
	mExtent = extent;
}

void Swapchain::CreateImageViews() {
	mImageViews.resize(mImages.size());

	for (uint32_t i = 0; i < mImages.size(); i++) {
		mImageViews[i] = Utils::ImageUtils::CreateImageView(mImages[i], VK_IMAGE_VIEW_TYPE_2D, mFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);
	}
}

void Swapchain::CreateFramebuffers(VkImageView _colorView, VkImageView _depthView) {
	LOG_MSG("Creating frame buffers", LogVerbosity::Info);

	const App& appInst = App::GetInstance();

	mFramebuffers.resize(mImageViews.size());

	for (size_t i = 0; i < mImageViews.size(); i++) {
		std::array<VkImageView, 3> attachments = {
			_colorView,
			_depthView,
			mImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = appInst.GetRenderPass(0),
			.attachmentCount = static_cast<uint32_t>(attachments.size()),
			.pAttachments = attachments.data(),
			.width = mExtent.width,
			.height = mExtent.height,
			.layers = 1
		};

		if (vkCreateFramebuffer(appInst.GetLogicalDevice(), &framebufferInfo, nullptr, &mFramebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create framebuffer!");
		}
	}
}

void Swapchain::CreateSyncObjects() {
	VkSemaphoreCreateInfo semaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	// We require a semaphore for knowing when each swapchain image
	// has finished rendering, not each frame! Important distinction,
	// as it is possible that they mismatch
	mRenderFinishedSemaphores.resize(mImages.size());
	for (size_t i = 0; i < mImages.size(); i++) {
		if (vkCreateSemaphore(App::GetInstance().GetLogicalDevice(), &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create sync objects for a frame!");
		}
	}
}

void Swapchain::CleanupSwapchain() {
	LOG_MSG("Cleaning up swapchain", LogVerbosity::Info);

	const App& appInst = App::GetInstance();
	const VkDevice& logicalDevice = appInst.GetLogicalDevice();

	for (const auto& framebuffer : mFramebuffers) {
		vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
	}

	for (const auto& imageView : mImageViews) {
		vkDestroyImageView(logicalDevice, imageView, nullptr);
	}

	vkDestroySwapchainKHR(logicalDevice, mSwapchain, nullptr);
}

void Swapchain::CleanupSyncObjects() {
	const VkDevice& logicalDevice = App::GetInstance().GetLogicalDevice();

	// Since the frames in flight can mismatch the swapchain
	// image count, we need to destroy this separately
	for (size_t i = 0; i < mRenderFinishedSemaphores.size(); i++) {
		vkDestroySemaphore(logicalDevice, mRenderFinishedSemaphores[i], nullptr);
	}
}

SwapchainSupportDetails Swapchain::QuerySwapChainSupport(VkPhysicalDevice _device, VkSurfaceKHR _surface) const {
	SwapchainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_device, _surface, &details.capabilities);

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(_device, _surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(_device, _surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(_device, _surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(_device, _surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

VkResult Swapchain::AcquireNextImage(VkSemaphore _imageAvailableSemaphore, uint32_t& _outIndex) const {
	return vkAcquireNextImageKHR(
		App::GetInstance().GetLogicalDevice(), 
		mSwapchain, 
		UINT64_MAX, 
		_imageAvailableSemaphore, 
		VK_NULL_HANDLE, 
		&_outIndex
	);
}

VkSurfaceFormatKHR Swapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& _availableFormats) {
	for (const auto& availableFormat : _availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}

	// If no desired format is found, just default to the first one
	return _availableFormats[0];
}

VkPresentModeKHR Swapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& _availablePresentModes, bool _vsyncEnabled) {
	for (const auto& availablePresentMode : _availablePresentModes) {
		// Prefer specific present modes depending on vsync toggle
		if (_vsyncEnabled) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}
		else {
			if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
				return availablePresentMode;
			}
		}
	}

	// If no desired present mode is found, default to guaranteed one
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& _capabilities) {
	// Max value of uint32_t signifies that the pixel size of
	// the window does not match the 'currentExtent'
	if (_capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return _capabilities.currentExtent;
	}
	else {
		const glm::ivec2 windowExtents = glm::clamp(App::GetInstance().GetWindow().GetExtents(),
			glm::ivec2(_capabilities.minImageExtent.width, _capabilities.minImageExtent.height),
			glm::ivec2(_capabilities.maxImageExtent.width, _capabilities.maxImageExtent.height)
		);

		return VkExtent2D(windowExtents.x, windowExtents.y);
	}
}
