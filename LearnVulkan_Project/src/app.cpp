#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "pch.h"

#include "app.h"
#include "renderer/mesh.h"
#include "renderer/pipeline_layout_manager.h"
#include "renderer/shader.h"
#include "utils/vulkan_ext_funcs.h"

#include <algorithm>
#include <chrono>

static App* gInstance = nullptr;

const uint32_t gWidth = 800;
const uint32_t gHeight = 600;
const int gMaxFramesInFlight = 2;

const std::string gTexturePath = "assets/textures/viking_room.png";

const std::vector<const char*> gValidationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> gDeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool gEnableValidationLayers = false;
#else
const bool gEnableValidationLayers = true;
#endif

static void FramebufferResizeCallback(GLFWwindow* _window, int /*_width*/, int /*_height*/) {
	if (App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(_window))) {
		app->framebufferResized = true;
	}
}

void App::Run() {
	InitWindow();
	InitVulkan();
	MainLoop();
	Cleanup();
}

void App::InitWindow() {
	glfwInit();

	// Since GLFW was designed to create an OpenGL context,
	// we need to tell it to *not* create one
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// Resizing requires special care, so disable it for now
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	// 1,2,3 params are window width, height and title
	// 4th param => specifies which monitor to open window on
	// 5th param => specific to OpenGL
	mWindow = glfwCreateWindow(gWidth, gHeight, "Learn Vulkan", nullptr, nullptr);
	glfwSetWindowUserPointer(mWindow, this);
	glfwSetFramebufferSizeCallback(mWindow, FramebufferResizeCallback);
}

void App::InitVulkan() {
	CreateInstance();
	SetupDebugMessenger();
	CreateWindowSurface();
	PickPhysicalDevice();
	CreateLogicalDevice();

	PipelineLayoutManager::Init();

	CreateSwapChain();
	CreateImageViews();
	CreateRenderPass();
	
	mTempShader = new Shader(
		std::vector<ShaderStage>{
			ShaderStage{ .filePath = "assets/shaders/shader.vert.spv", .flagBit = VK_SHADER_STAGE_VERTEX_BIT },
			ShaderStage{ .filePath = "assets/shaders/shader.frag.spv", .flagBit = VK_SHADER_STAGE_FRAGMENT_BIT }
		},
		BlendModel::Opaque,
		ShadingModel::Unlit
	);

	CreateCommandPool();
	CreateColorResources();
	CreateDepthResources();
	CreateFramebuffers();
	CreateTextureImage();
	CreateTextureImageView();
	CreateTextureSampler();
	
	mTempMesh = new Mesh("assets/models/viking_room.obj");
	mTempMesh->SetShader(mTempShader);

	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();
	CreateCommandBuffers();
	CreateSyncObjects();
}

void App::CreateInstance() {
	// First thing's first, check for validation layer support
	if (gEnableValidationLayers && !CheckValidationLayerSupport()) {
		throw std::runtime_error("Validation layers requested, but not available!");
	}

	// This data is technically optional, but can help
	// the GPU optimize for our specific application
	VkApplicationInfo appInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Learn Vulkan",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "No Engine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_0
	};

	// Following is mandatory, it tells Vulkan which global extensions
	// and validation layers we want to use for our application
	// 'global' just means it applies to the entire program

	std::vector<const char*> requiredExtensions = GetRequiredExtensions();

	VkInstanceCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo,
		.enabledLayerCount = 0,
		.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
		.ppEnabledExtensionNames = requiredExtensions.data(),
	};

	// Conditionally enable validation layers, and the debug messenger
	// We store this here, so that it is not destroyed before 'vkCreateInstance'
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
	if (gEnableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(gValidationLayers.size());
		createInfo.ppEnabledLayerNames = gValidationLayers.data();

		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)(&debugCreateInfo);
	}
	else {
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	VkResult createInstResult = vkCreateInstance(&createInfo, nullptr, &mInstance);
	if (createInstResult != VK_SUCCESS) {
		if (createInstResult == VK_ERROR_EXTENSION_NOT_PRESENT) {
			PrintUnsupportedExtensions(requiredExtensions);
		}
		
		throw std::runtime_error("Failed to create instance!");
	}
}

std::vector<const char*> App::GetRequiredExtensions() {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	// This is for adding a custom message callback for validation layers
	if (gEnableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

void App::PrintUnsupportedExtensions(std::vector<const char*>& _extensions) {
	uint32_t supportedCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &supportedCount, nullptr);

	std::vector<VkExtensionProperties> supportedExt(supportedCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &supportedCount, supportedExt.data());

	// Check if all required extensions are supported
	std::set<std::string> supportedExtSet = {};
	for (const auto& ext : supportedExt) {
		supportedExtSet.insert(ext.extensionName);
	}

	for (const auto& requiredName : _extensions) {
		if (supportedExtSet.contains(requiredName)) {
			std::cerr << "Unsupported extension: " << requiredName << "\n";
		}
	}
}

bool App::CheckValidationLayerSupport() {
	uint32_t supportedCount = 0;
	vkEnumerateInstanceLayerProperties(&supportedCount, nullptr);

	std::vector<VkLayerProperties> supportedLayers(supportedCount);
	vkEnumerateInstanceLayerProperties(&supportedCount, supportedLayers.data());

	// Check if all required validation layers are supported
	std::set<std::string> supportedLayerSet = {};
	for (const auto& layer : supportedLayers) {
		supportedLayerSet.insert(layer.layerName);
	}

	bool allValidLayers = true;

	for (const auto& layerName : gValidationLayers) {
		if (!supportedLayerSet.contains(std::string(layerName))) {
			std::cerr << "Unsupported validation layer: " << layerName << "\n";
			allValidLayers = false;
		}
	}

	return allValidLayers;
}

void App::SetupDebugMessenger() {
	if (!gEnableValidationLayers) {
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
	PopulateDebugMessengerCreateInfo(createInfo);

	if (Utils::VulkanExtFuncs::CreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("Failed to set up debug messenger!");
	}
}

void App::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& _createInfo) {
	// Here is where we specify which message severity and types
	// we would like our callback to be called for
	_createInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = Utils::VulkanExtFuncs::DebugCallback,
		.pUserData = nullptr // optional, for any custom user data
	};
}

void App::CreateWindowSurface() {
	if (glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mWindowSurface) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create window surface!");
	}
}

VkSampleCountFlagBits App::GetMaxUsableSampleCount() const {
	VkPhysicalDeviceProperties physicalDeviceProperties = {};
	vkGetPhysicalDeviceProperties(mPhysicalDevice, &physicalDeviceProperties);

	VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts 
		& physicalDeviceProperties.limits.framebufferDepthSampleCounts;

	if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
	if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
	if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
	if (counts & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
	if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
	if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;

	return VK_SAMPLE_COUNT_1_BIT;
}

void App::PickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		throw std::runtime_error("Failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

	for (const auto& device : devices) {
		if (IsDeviceSuitable(device)) {
			mPhysicalDevice = device;
			mMsaaState.samples = GetMaxUsableSampleCount();
			break;
		}
	}

	if (mPhysicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("Failed to find a suitable GPU!");
	}
}

bool App::IsDeviceSuitable(VkPhysicalDevice _device) {
	QueueFamilyIndices indices = FindQueueFamilies(_device);

	bool extensionsSupported = CheckDeviceExtensionSupport(_device);

	bool swapChainAdequate = false;
	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(_device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	VkPhysicalDeviceFeatures supportedFeatures = {};
	vkGetPhysicalDeviceFeatures(_device, &supportedFeatures);

	return indices.IsComplete() 
		&& extensionsSupported 
		&& swapChainAdequate
		&& supportedFeatures.samplerAnisotropy;
}

bool App::CheckDeviceExtensionSupport(VkPhysicalDevice _device) {
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(_device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(_device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(gDeviceExtensions.begin(), gDeviceExtensions.end());

	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

QueueFamilyIndices App::FindQueueFamilies(VkPhysicalDevice _device) const {
	QueueFamilyIndices indices = {};

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(_device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(_device, &queueFamilyCount, queueFamilies.data());

	// Find any queue family that supports 'VK_QUEUE_GRAPHICS_BIT'
	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(_device, i, mWindowSurface, &presentSupport);

		if (presentSupport) {
			indices.presentFamily = i;
		}

		// Found queue family, exit early
		if (indices.IsComplete()) {
			break;
		}

		i++;
	}

	return indices;
}

void App::CreateLogicalDevice() {
	QueueFamilyIndices indices = FindQueueFamilies(mPhysicalDevice);

	// Set priority of queue, this influences the scheduling of
	// command buffer execution, and is required even for a single queue
	float queuePriority = 1.f;

	// Create and store all unique queue families, these
	// are then pointed to in logical device create info
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = {};
	std::set<uint32_t> uniqueQueueFamilies = { 
		indices.graphicsFamily.value(), 
		indices.presentFamily.value()
	};
	
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = queueFamily,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority
		};

		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Specify what device features we want to use, 
	// by default sets all feature flags to VK_FALSE
	VkPhysicalDeviceFeatures deviceFeatures = {
		.samplerAnisotropy = VK_TRUE
	};

	VkDeviceCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
		.pQueueCreateInfos = queueCreateInfos.data(),
		.enabledExtensionCount = static_cast<uint32_t>(gDeviceExtensions.size()),
		.ppEnabledExtensionNames = gDeviceExtensions.data(),
		.pEnabledFeatures = &deviceFeatures,
	};

	// Check for validation layer support, this is no longer necessary in
	// Vulkan and only needed for compatibility with older implementations
	if (gEnableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(gValidationLayers.size());
		createInfo.ppEnabledLayerNames = gValidationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mLogicalDevice) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create logical device!");
	}

	// Finally store the queue handles
	vkGetDeviceQueue(mLogicalDevice, indices.graphicsFamily.value(), 0, &mGraphicsQueue);
	vkGetDeviceQueue(mLogicalDevice, indices.presentFamily.value(), 0, &mPresentQueue);
}

SwapChainSupportDetails App::QuerySwapChainSupport(VkPhysicalDevice _device) const {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_device, mWindowSurface, &details.capabilities);
	
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(_device, mWindowSurface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(_device, mWindowSurface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(_device, mWindowSurface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(_device, mWindowSurface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

VkSurfaceFormatKHR App::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& _availableFormats) {
	for (const auto& availableFormat : _availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}

	// If no desired format is found, just default to the first one
	return _availableFormats[0];
}

VkPresentModeKHR App::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& _availablePresentModes) {
	for (const auto& availablePresentMode : _availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
	}

	// If no desired present mode is found, default to guaranteed one
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D App::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& _capabilities) {
	// Max value of uint32_t signifies that the pixel size of
	// the window does not match the 'currentExtent'
	if (_capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return _capabilities.currentExtent;
	}
	else {
		int width = 0, height = 0;
		glfwGetFramebufferSize(mWindow, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height),
		};

		actualExtent.width = std::clamp(actualExtent.width, 
			_capabilities.minImageExtent.width, 
			_capabilities.maxImageExtent.width
		);

		actualExtent.height = std::clamp(actualExtent.height,
			_capabilities.minImageExtent.height,
			_capabilities.maxImageExtent.height
		);

		return actualExtent;
	}
}

void App::CreateSwapChain() {
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(mPhysicalDevice);
	
	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

	// This is the minimum number of images that are required to function,
	// however using this number may stall our program, so +1 is recommended
	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

	// We need to clamp to the max image count, but also need
	// to check if '0' is the max, which specifies no maximum
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = mWindowSurface,
		.minImageCount = imageCount,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1, // always 1, unless making a 3d app
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
	};

	QueueFamilyIndices indices = FindQueueFamilies(mPhysicalDevice);
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

	if (vkCreateSwapchainKHR(mLogicalDevice, &createInfo, nullptr, &mSwapChain) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create swap chain!");
	}

	// Store swap chain image handles
	vkGetSwapchainImagesKHR(mLogicalDevice, mSwapChain, &imageCount, nullptr);
	mSwapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(mLogicalDevice, mSwapChain, &imageCount, mSwapChainImages.data());

	// And also store image format + extent for future use
	mSwapChainImageFormat = surfaceFormat.format;
	mSwapChainExtent = extent;
}

VkImageView App::CreateImageView(VkImage _image, VkFormat _format, VkImageAspectFlags _aspectFlags, uint32_t _mipLevels) const {
	VkImageViewCreateInfo viewInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = _image,

		// These specify how the image data is interpreted
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = _format,

		// This describes what the image purpose is, and which
		// part of the image should be accessed. For now our image
		// contains no mip levels or multiple layers
		.subresourceRange = {
			.aspectMask = _aspectFlags,
			.baseMipLevel = 0,
			.levelCount = _mipLevels,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	VkImageView imageView = {};
	if (vkCreateImageView(mLogicalDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create image view!");
	}

	return imageView;
}

void App::CreateImageViews() {
	mSwapChainImageViews.resize(mSwapChainImages.size());

	for (uint32_t i = 0; i < mSwapChainImages.size(); i++) {
		mSwapChainImageViews[i] = CreateImageView(mSwapChainImages[i], mSwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}
}

void App::CreateRenderPass() {
	VkAttachmentDescription colorAttachment = {
		.format = mSwapChainImageFormat,
		.samples = mMsaaState.samples,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkAttachmentReference colorAttachmentRef = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkAttachmentDescription depthAttachment = {
		.format = FindDepthFormat(),
		.samples = mMsaaState.samples,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,

		// We do not care about storing the depth data across frames
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	VkAttachmentReference depthAttachmentRef = {
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	VkAttachmentDescription colorAttachmentResolve = {
		.format = mSwapChainImageFormat,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};

	VkAttachmentReference colorAttachmentResolveRef = {
		.attachment = 2,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription subpass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentRef,

		// Point to the resolve attachment for multisampling
		.pResolveAttachments = &colorAttachmentResolveRef,

		// We do not specify a count of depth-stencil attachments, 
		// as a subpass can only hold a single one
		.pDepthStencilAttachment = &depthAttachmentRef,
	};

	VkSubpassDependency dependency = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
	};

	std::array<VkAttachmentDescription, 3> attachments = { 
		colorAttachment, 
		depthAttachment,
		colorAttachmentResolve
	};

	VkRenderPassCreateInfo renderPassInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	};

	// One render pass for now
	mRenderPasses.push_back({});

	if (vkCreateRenderPass(mLogicalDevice, &renderPassInfo, nullptr, &mRenderPasses[0]) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create render pass!");
	}
}

VkShaderModule App::CreateShaderModule(const std::vector<char>& _code) const {
	VkShaderModuleCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = _code.size(),
		.pCode = reinterpret_cast<const uint32_t*>(_code.data())
	};

	VkShaderModule shaderModule = {};
	if (vkCreateShaderModule(mLogicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create shader module!");
	}

	return shaderModule;
}

void App::CreateFramebuffers() {
	mSwapChainFramebuffers.resize(mSwapChainImageViews.size());

	for (size_t i = 0; i < mSwapChainImageViews.size(); i++) {
		std::array<VkImageView, 3> attachments = {
			mColorImageView,
			mDepthImageView,
			mSwapChainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = mRenderPasses[0],
			.attachmentCount = static_cast<uint32_t>(attachments.size()),
			.pAttachments = attachments.data(),
			.width = mSwapChainExtent.width,
			.height = mSwapChainExtent.height,
			.layers = 1
		};

		if (vkCreateFramebuffer(mLogicalDevice, &framebufferInfo, nullptr, &mSwapChainFramebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create framebuffer!");
		}
	}
}

void App::CreateCommandPool() {
	QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(mPhysicalDevice);

	VkCommandPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()
	};

	if (vkCreateCommandPool(mLogicalDevice, &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create command pool!");
	}
}

void App::CreateBuffer(VkDeviceSize _size, VkBufferUsageFlags _usage, VkMemoryPropertyFlags _properties, VkBuffer& _buffer, VkDeviceMemory& _bufferMemory) const {
	VkBufferCreateInfo bufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = _size,
		.usage = _usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	if (vkCreateBuffer(mLogicalDevice, &bufferInfo, nullptr, &_buffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create buffer!");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetBufferMemoryRequirements(mLogicalDevice, _buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, _properties)
	};

	if (vkAllocateMemory(mLogicalDevice, &allocInfo, nullptr, &_bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate buffer memory!");
	}

	vkBindBufferMemory(mLogicalDevice, _buffer, _bufferMemory, 0);
}

void App::CopyBuffer(VkBuffer _srcBuffer, VkBuffer _dstBuffer, VkDeviceSize _size) const {
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	// Record copy command
	{
		VkBufferCopy copyRegion = {
			.srcOffset = 0, // optional
			.dstOffset = 0, // optional
			.size = _size,
		};

		vkCmdCopyBuffer(commandBuffer, _srcBuffer, _dstBuffer, 1, &copyRegion);
	}
	
	EndSingleTimeCommands(commandBuffer);
}

void App::CreateImage(uint32_t _width, uint32_t _height, uint32_t _mipLevels, VkSampleCountFlagBits _numSamples, VkFormat _format, VkImageTiling _tiling, VkImageUsageFlags _usage, VkMemoryPropertyFlags _properties, VkImage& _image, VkDeviceMemory& _imageMemory) const {
	VkImageCreateInfo imageInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = _format,
		.extent = {
			.width = _width,
			.height = _height,
			.depth = 1
		},
		.mipLevels = _mipLevels,
		.arrayLayers = 1,
		.samples = _numSamples,
		.tiling = _tiling,
		.usage = _usage,

		// Image only used by one queue family
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,

		// We do not care about the initial layout, as we are copying to it
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	if (vkCreateImage(mLogicalDevice, &imageInfo, nullptr, &_image) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create image!");
	}

	VkMemoryRequirements memRequirements = {};
	vkGetImageMemoryRequirements(mLogicalDevice, _image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, _properties)
	};

	if (vkAllocateMemory(mLogicalDevice, &allocInfo, nullptr, &_imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate image memory!");
	}

	vkBindImageMemory(mLogicalDevice, _image, _imageMemory, 0);
}

void App::TransitionImageLayout(VkImage _image, VkFormat /*_format*/, VkImageLayout _oldLayout, VkImageLayout _newLayout, uint32_t _mipLevels) {
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	VkImageMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.oldLayout = _oldLayout,
		.newLayout = _newLayout,

		// Indicate we are not transferring queue family ownership,
		// this is not the default behaviour!
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

		.image = _image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = _mipLevels,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
	};

	VkPipelineStageFlags srcStage = {}, dstStage = {};

	// Undefined => transfer destination
	if (_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && _newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	// Transfer destination => shader reading
	else if (_oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && _newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	// Unknown
	else {
		throw std::invalid_argument("Unsupported layout transition!");
	}

	vkCmdPipelineBarrier(
		commandBuffer, 
		srcStage, dstStage,
		0, 
		0, nullptr, // memory barriers
		0, nullptr, // buffer memory barries
		1, &barrier // image memory barriers
	);

	EndSingleTimeCommands(commandBuffer);
}

void App::CopyBufferToImage(VkBuffer _buffer, VkImage _image, uint32_t _width, uint32_t _height) {
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	// Specify the region of the buffer copied to which part of the image
	VkBufferImageCopy region = {
		// Byte offset in the buffer that pixel data starts
		.bufferOffset = 0,

		// How are pixels laid out in memory?
		.bufferRowLength = 0,
		.bufferImageHeight = 0,

		// Which part of the image are we copying the pixels into?
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
		.imageOffset = { 0, 0, 0 },
		.imageExtent = { _width, _height, 1 }
	};

	vkCmdCopyBufferToImage(
		commandBuffer,
		_buffer,
		_image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	EndSingleTimeCommands(commandBuffer);
}

VkFormat App::FindSupportedFormat(const std::vector<VkFormat>& _candidates, VkImageTiling _tiling, VkFormatFeatureFlags _features) const {
	for (VkFormat format : _candidates) {
		VkFormatProperties props = {};
		vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, format, &props);

		if (_tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & _features) == _features) {
			return format;
		}
		else if (_tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & _features) == _features) {
			return format;
		}
	}

	throw std::runtime_error("Failed to find supported format!");
}

void App::CreateColorResources() {
	VkFormat colorFormat = mSwapChainImageFormat;

	CreateImage(
		mSwapChainExtent.width,
		mSwapChainExtent.height,
		1,
		mMsaaState.samples,
		colorFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mColorImage,
		mColorImageMemory
	);

	mColorImageView = CreateImageView(mColorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);


}

VkFormat App::FindDepthFormat() {
	return FindSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

bool App::HasStencilComponent(VkFormat _format) {
	return _format == VK_FORMAT_D32_SFLOAT_S8_UINT || _format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void App::CreateDepthResources() {
	VkFormat depthFormat = FindDepthFormat();

	CreateImage(
		mSwapChainExtent.width,
		mSwapChainExtent.height,
		1,
		mMsaaState.samples,
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mDepthImage,
		mDepthImageMemory
	);

	mDepthImageView = CreateImageView(mDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
}

void App::GenerateMipMaps(VkImage _image, VkFormat _imageFormat, int32_t _texWidth, int32_t _texHeight, uint32_t _mipLevels) {
	// Check if image format supports linear blitting
	VkFormatProperties formatProperties = {};
	vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, _imageFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("Texture image format does not support linear blitting!");
	}
	
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	VkImageMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = _image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};

	int32_t mipWidth = _texWidth, mipHeight = _texHeight;

	for (uint32_t i = 1; i < mMipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr, // memory barriers
			0, nullptr, // buffer memory barriers
			1, &barrier // image memory barriers
		);

		// Specify the regions that will be copied from / to
		VkImageBlit blit = {
			.srcSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = i - 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
			.srcOffsets = {
				{ 0, 0, 0 },
				{ mipWidth, mipHeight, 1 }
			},
			.dstSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = i,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
			.dstOffsets = {
				{ 0, 0, 0 },
				{ (mipWidth > 1 ? mipWidth / 2 : 1), (mipHeight > 1 ? mipHeight / 2 : 1), 1 }
			},
		};

		vkCmdBlitImage(commandBuffer,
			_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR
		);

		// Transition mip level to shader read only
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr, // memory barriers
			0, nullptr, // buffer memory barriers
			1, &barrier // image memory barriers
		);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	// Transition mip level to shader read only
	barrier.subresourceRange.baseMipLevel = _mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr, // memory barriers
		0, nullptr, // buffer memory barriers
		1, &barrier // image memory barriers
	);

	EndSingleTimeCommands(commandBuffer);
}

void App::CreateTextureImage() {
	int texWidth = 0, texHeight = 0, texChannels = 0;
	stbi_uc* pixels = stbi_load(gTexturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	// 4 bytes per pixel
	VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth * texHeight * 4);

	if (!pixels) {
		throw std::runtime_error("Failed to load texture image!");
	}

	// Create staging buffer
	VkBuffer stagingBuffer = {};
	VkDeviceMemory stagingBufferMemory = {};
	CreateBuffer(
		imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	// Copy pixel data into device local memory
	void* data;
	vkMapMemory(mLogicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(mLogicalDevice, stagingBufferMemory);

	// Free pixel data
	stbi_image_free(pixels);

	mMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

	CreateImage(
		texWidth, 
		texHeight,
		mMipLevels,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_SRGB,

		// Image object is optimal for shader access
		VK_IMAGE_TILING_OPTIMAL,

		// Source / destination for a buffer copy, and sampled in the shader
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,

		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mTextureImage,
		mTextureImageMemory
	);

	// Copy the staging buffer into the texture image
	TransitionImageLayout(
		mTextureImage, 
		VK_FORMAT_R8G8B8A8_SRGB, 
		VK_IMAGE_LAYOUT_UNDEFINED, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		mMipLevels
	);

	CopyBufferToImage(
		stagingBuffer,
		mTextureImage,
		static_cast<uint32_t>(texWidth),
		static_cast<uint32_t>(texHeight)
	);

	// Transition image for shader access is handled
	// for each mip level when generating them
	GenerateMipMaps(mTextureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mMipLevels);

	vkDestroyBuffer(mLogicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(mLogicalDevice, stagingBufferMemory, nullptr);
}

void App::CreateTextureImageView() {
	mTextureImageView = CreateImageView(mTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, mMipLevels);
}

void App::CreateTextureSampler() {
	VkSamplerCreateInfo samplerInfo = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR, // concerns oversampling
		.minFilter = VK_FILTER_LINEAR, // concerns undersampling

		// What to do when sampling outside the texture borders
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,

		// Enable anisotropy, as it is relatively cheap nowadays
		.anisotropyEnable = VK_TRUE,

		// If a comparision function is enabled, texels are first compared
		// to some value and the result of the comparison is used for filtering
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,

		// Border color when sampling outside the texture borders
		// and address mode is 'clamp to border'
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE,

		// If true, sampler uses [0..texWidth] and [0..texHeight] for
		// sampling texture coordinates. If false, simply use [0..1]
		// Note, most applications keep this false so UVs are [0..1]
		.unnormalizedCoordinates = VK_FALSE,
	};

	// We define these here because they are not in a sensible order
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.f;
	samplerInfo.minLod = 0.f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE; // no max lod

	// Query max anisotropy supported by device
	VkPhysicalDeviceProperties properties = {};
	vkGetPhysicalDeviceProperties(mPhysicalDevice, &properties);

	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

	if (vkCreateSampler(mLogicalDevice, &samplerInfo, nullptr, &mTextureSampler) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create texture sampler!");
	}
}

void App::CreateUniformBuffers() {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	mUniformBuffers.resize(gMaxFramesInFlight);
	mUniformBuffersMemory.resize(gMaxFramesInFlight);
	mUniformBuffersMapped.resize(gMaxFramesInFlight);

	for (size_t i = 0; i < gMaxFramesInFlight; i++) {
		CreateBuffer(
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			mUniformBuffers[i],
			mUniformBuffersMemory[i]
		);

		vkMapMemory(mLogicalDevice, mUniformBuffersMemory[i], 0, bufferSize, 0, &mUniformBuffersMapped[i]);
	}
}

void App::CreateDescriptorPool() {
	// What type of descriptors our sets will contain, and how many
	std::array<VkDescriptorPoolSize, 2> poolSizes = {};

	poolSizes[0] = {
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = static_cast<uint32_t>(gMaxFramesInFlight)
	};

	poolSizes[1] = {
		.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = static_cast<uint32_t>(gMaxFramesInFlight)
	};

	// We have already said how many descriptors are available,
	// but we also need to specify how many descriptor sets may
	// be allocated via '.maxSets'
	VkDescriptorPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = static_cast<uint32_t>(gMaxFramesInFlight),
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	if (vkCreateDescriptorPool(mLogicalDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create descriptor pool!");
	}
}

void App::CreateDescriptorSets() {
	const VkDescriptorSetLayout& globalLayout = PipelineLayoutManager::GetInstance().GetDescriptorSetLayout(DescriptorSet::Global);
	std::vector<VkDescriptorSetLayout> layouts(gMaxFramesInFlight, globalLayout);

	VkDescriptorSetAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = mDescriptorPool,
		.descriptorSetCount = static_cast<uint32_t>(gMaxFramesInFlight),
		.pSetLayouts = layouts.data()
	};

	mDescriptorSets.resize(gMaxFramesInFlight);
	if (vkAllocateDescriptorSets(mLogicalDevice, &allocInfo, mDescriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < gMaxFramesInFlight; i++) {
		// Which buffer? And where the region within it
		// that contains the data for the descriptor
		VkDescriptorBufferInfo bufferInfo = {
			.buffer = mUniformBuffers[i],
			.offset = 0,
			.range = sizeof(UniformBufferObject)
		};

		// Which image and sampler? And the layout of the image
		VkDescriptorImageInfo imageInfo = {
			.sampler = mTextureSampler,
			.imageView = mTextureImageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

		descriptorWrites[0] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = mDescriptorSets[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &bufferInfo
		};

		descriptorWrites[1] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = mDescriptorSets[i],
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &imageInfo
		};

		// Apply updates - this accepts write arrays, and copy arrays
		vkUpdateDescriptorSets(
			mLogicalDevice, 

			// Write array
			static_cast<uint32_t>(descriptorWrites.size()), 
			descriptorWrites.data(), 

			// Copy array
			0, 
			nullptr
		);
	}
}

uint32_t App::FindMemoryType(uint32_t _typeFilter, VkMemoryPropertyFlags _properties) const {
	VkPhysicalDeviceMemoryProperties memProperties = {};
	vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		bool suitableMemoryType = (_typeFilter & (1 << i));
		bool allPropertiesSupported = (memProperties.memoryTypes[i].propertyFlags & _properties);

		if (suitableMemoryType && allPropertiesSupported) {
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type!");
}

void App::CreateCommandBuffers() {
	mCommandBuffers.resize(gMaxFramesInFlight);

	VkCommandBufferAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = mCommandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = (uint32_t)(mCommandBuffers.size())
	};

	if (vkAllocateCommandBuffers(mLogicalDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate command buffers!");
	}
}

VkCommandBuffer App::BeginSingleTimeCommands() const {
	VkCommandBufferAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = mCommandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(mLogicalDevice, &allocInfo, &commandBuffer);

	// Tell the driver that we are going to wait until the copy
	// operation has finished executing using the right flag
	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void App::EndSingleTimeCommands(VkCommandBuffer _commandBuffer) const {
	vkEndCommandBuffer(_commandBuffer);

	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &_commandBuffer
	};

	vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(mGraphicsQueue);

	vkFreeCommandBuffers(mLogicalDevice, mCommandPool, 1, &_commandBuffer);
}

void App::RecordCommandBuffer(VkCommandBuffer _commandBuffer, uint32_t _imageIndex) const {
	VkCommandBufferBeginInfo beginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = 0, // optional
			.pInheritanceInfo = nullptr // optional
	};

	if (vkBeginCommandBuffer(_commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("Failed to begin recording command buffer!");
	}

	// Clear values for our swapchain images (color, depth, etc.)
	std::array<VkClearValue, 2> clearValues = {};
	clearValues[0].color = { 0.f, 0.f, 0.f, 1.f };
	clearValues[1].depthStencil = { 1.f, 0 };

	VkRenderPassBeginInfo renderPassInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = mRenderPasses[0],
		.framebuffer = mSwapChainFramebuffers[_imageIndex],
		.renderArea = {
			.offset = { 0, 0 },
			.extent = mSwapChainExtent
		},
		.clearValueCount = static_cast<uint32_t>(clearValues.size()),
		.pClearValues = clearValues.data()
	};

	// -- Render Pass --
	vkCmdBeginRenderPass(_commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	{
		// This is where the shader (pipeline) and vertex/index buffers get bound for this frame
		mTempMesh->BindMeshResources(_commandBuffer);

		// Bind descriptor sets - this is for uniform buffers

		// Eventually, this will only be called at the beginning of each sub-pass,
		// where we also will switch pipeline layout
		const VkPipelineLayout& layout = PipelineLayoutManager::GetInstance().GetLayoutForModel(BlendModel::Opaque, ShadingModel::Unlit);
		vkCmdBindDescriptorSets(_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &mDescriptorSets[mCurrentFrame], 0, nullptr);

		// Since viewport and scissor state are dynamic, we need to
		// explicitly set them before we can issue a draw command
		VkViewport viewport = {
			.x = 0.f,
			.y = 0.f,
			.width = static_cast<float>(mSwapChainExtent.width),
			.height = static_cast<float>(mSwapChainExtent.height),
			.minDepth = 0.f,
			.maxDepth = 1.f
		};
		vkCmdSetViewport(_commandBuffer, 0, 1, &viewport);

		VkRect2D scissor = {
			.offset = { 0, 0 },
			.extent = mSwapChainExtent
		};
		vkCmdSetScissor(_commandBuffer, 0, 1, &scissor);

		// This is where the actual draw call happens for our mesh
		mTempMesh->DrawMesh(_commandBuffer);
	}
	vkCmdEndRenderPass(_commandBuffer);

	if (vkEndCommandBuffer(_commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to record command buffer!");
	}
}

void App::CreateSyncObjects() {
	mImageAvailableSemaphores.resize(gMaxFramesInFlight);
	mInFlightFences.resize(gMaxFramesInFlight);

	VkSemaphoreCreateInfo semaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	// Create the fence in the signalled state, so that on the
	// first frame we do not wait infinitely
	VkFenceCreateInfo fenceInfo = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};

	for (size_t i = 0; i < gMaxFramesInFlight; i++) {
		if (vkCreateSemaphore(mLogicalDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(mLogicalDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create sync objects for a frame!");
		}
	}

	// We require a semaphore for knowing when each swapchain image
	// has finished rendering, not each frame! Important distinction,
	// as it is possible that they mismatch
	mRenderFinishedSemaphores.resize(mSwapChainImages.size());
	for (size_t i = 0; i < mSwapChainImages.size(); i++) {
		if (vkCreateSemaphore(mLogicalDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create sync objects for a frame!");
		}
	}
}

void App::RecreateSwapChain() {
	// If GLFW window is minimized, the framebuffer has size 0,
	// for now we just pause rendering until the size is non-zero
	int width = 0, height = 0;
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(mWindow, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(mLogicalDevice);

	CleanupSwapChain();

	CreateSwapChain();
	CreateImageViews();
	CreateColorResources();
	CreateDepthResources();
	CreateFramebuffers();
}

void App::CleanupSwapChain() {
	vkDestroyImageView(mLogicalDevice, mColorImageView, nullptr);
	vkDestroyImage(mLogicalDevice, mColorImage, nullptr);
	vkFreeMemory(mLogicalDevice, mColorImageMemory, nullptr);

	vkDestroyImageView(mLogicalDevice, mDepthImageView, nullptr);
	vkDestroyImage(mLogicalDevice, mDepthImage, nullptr);
	vkFreeMemory(mLogicalDevice, mDepthImageMemory, nullptr);

	for (const auto& framebuffer : mSwapChainFramebuffers) {
		vkDestroyFramebuffer(mLogicalDevice, framebuffer, nullptr);
	}

	for (const auto& imageView : mSwapChainImageViews) {
		vkDestroyImageView(mLogicalDevice, imageView, nullptr);
	}

	vkDestroySwapchainKHR(mLogicalDevice, mSwapChain, nullptr);
}

void App::MainLoop() {
	while (!glfwWindowShouldClose(mWindow)) {
		glfwPollEvents();
		DrawFrame();
	}

	// Wait for the logical device to finish operations
	vkDeviceWaitIdle(mLogicalDevice);
}

void App::DrawFrame() {
	// Wait for all fences in an array, for uint64_t::max timeout
	vkWaitForFences(mLogicalDevice, 1, &mInFlightFences[mCurrentFrame], VK_TRUE, UINT64_MAX);

	// Acquire image from swapchain, and only signal semaphore on completion
	uint32_t imageIndex = 0;
	VkResult result = vkAcquireNextImageKHR(mLogicalDevice, mSwapChain, UINT64_MAX, mImageAvailableSemaphores[mCurrentFrame], VK_NULL_HANDLE, &imageIndex);

	// Swapchain is out of date and must be recreated,
	// i.e window has been resized
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		RecreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("Failed to acquire swapchain image!");
	}

	// Manually reset our fences, only if we are submitting work
	vkResetFences(mLogicalDevice, 1, &mInFlightFences[mCurrentFrame]);

	UpdateUniformBuffer(mCurrentFrame);

	// Reset + record our command buffer
	vkResetCommandBuffer(mCommandBuffers[mCurrentFrame], 0);
	RecordCommandBuffer(mCommandBuffers[mCurrentFrame], imageIndex);

	VkSemaphore waitSemaphores[] = {
		mImageAvailableSemaphores[mCurrentFrame]
	};

	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	VkSemaphore signalSemaphores[] = {
		// Use actual swapchain image index for rendering semaphore
		mRenderFinishedSemaphores[imageIndex]
	};
	
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,

		// Specify which semaphores to wait on before execution begins,
		// and in which specific stages to do the waiting
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = waitSemaphores,
		.pWaitDstStageMask = waitStages,

		// Which command buffers to submit for execution
		.commandBufferCount = 1,
		.pCommandBuffers = &mCommandBuffers[mCurrentFrame],

		// Which semaphores to signal when command buffers
		// have finished their execution
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = signalSemaphores,
	};

	// Submit drawing commands, and signal our fence when done
	if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mInFlightFences[mCurrentFrame]) != VK_SUCCESS) {
		throw std::runtime_error("Failed to submit draw command buffer!");
	}

	VkSwapchainKHR swapChains[] = {
		mSwapChain
	};

	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = signalSemaphores,

		// Swapchains to present images to, and the image
		// index for each swapchain
		.swapchainCount = 1,
		.pSwapchains = swapChains,
		.pImageIndices = &imageIndex,

		// A results array, to check for individual swapchain results
		.pResults = nullptr // optional
	};

	// Present image back to swapchain
	result = vkQueuePresentKHR(mPresentQueue, &presentInfo);

	// Check if swapchain needs to be recreated or if presenting failed
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
		framebufferResized = false;
		RecreateSwapChain();
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to present swap chain image!");
	}

	// Advance to next frame
	mCurrentFrame = (mCurrentFrame + 1) % gMaxFramesInFlight;
}

void App::UpdateUniformBuffer(uint32_t _currentImage) {
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject ubo = {};
	ubo.model = glm::rotate(
		glm::mat4(1.f),						// inital matrix
		deltaTime * glm::radians(90.f),		// rotation (in radians)
		glm::vec3(0.f, 0.f, 1.f)			// axis of rotation
	);

	ubo.view = glm::lookAt(
		glm::vec3(2.f),				// camera position
		glm::vec3(0.f),				// position to look at
		glm::vec3(0.f, 0.f, 1.f)	// up axis
	);

	float aspectRatio = mSwapChainExtent.width / (float)(mSwapChainExtent.height);
	ubo.proj = glm::perspective(
		glm::radians(45.f),		// fov
		aspectRatio,
		0.1f,					// near clip
		10.f					// far clip plane
	);

	// Account for OpenGL flipped y coordinate, if you
	// don't do this, the image is rendered upside down
	ubo.proj[1][1] *= -1;

	// Copy ubo directly into already mapped buffer
	memcpy(mUniformBuffersMapped[_currentImage], &ubo, sizeof(ubo));
}

void App::Cleanup() {
	CleanupSwapChain();

	vkDestroySampler(mLogicalDevice, mTextureSampler, nullptr);
	vkDestroyImageView(mLogicalDevice, mTextureImageView, nullptr);
	vkDestroyImage(mLogicalDevice, mTextureImage, nullptr);
	vkFreeMemory(mLogicalDevice, mTextureImageMemory, nullptr);

	for (size_t i = 0; i < gMaxFramesInFlight; i++) {
		vkDestroyBuffer(mLogicalDevice, mUniformBuffers[i], nullptr);
		vkFreeMemory(mLogicalDevice, mUniformBuffersMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(mLogicalDevice, mDescriptorPool, nullptr);

	delete mTempMesh;

	for (size_t i = 0; i < mRenderPasses.size(); i++) {
		vkDestroyRenderPass(mLogicalDevice, mRenderPasses[i], nullptr);
	}

	for (size_t i = 0; i < gMaxFramesInFlight; i++) {
		vkDestroySemaphore(mLogicalDevice, mImageAvailableSemaphores[i], nullptr);
		vkDestroyFence(mLogicalDevice, mInFlightFences[i], nullptr);
	}

	// Since the frames in flight can mismatch the swapchain
	// image count, we need to destroy this separately
	for (size_t i = 0; i < mSwapChainImages.size(); i++) {
		vkDestroySemaphore(mLogicalDevice, mRenderFinishedSemaphores[i], nullptr);
	}

	vkDestroyCommandPool(mLogicalDevice, mCommandPool, nullptr);

	PipelineLayoutManager::Shutdown();
	delete mTempShader;

	vkDestroyDevice(mLogicalDevice, nullptr);

	if (gEnableValidationLayers) {
		Utils::VulkanExtFuncs::DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
	}

	vkDestroySurfaceKHR(mInstance, mWindowSurface, nullptr);
	vkDestroyInstance(mInstance, nullptr);

	glfwDestroyWindow(mWindow);
	glfwTerminate();
}
