#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "pch.h"

#include "app.h"
#include "renderer/material.h"
#include "renderer/mesh.h"
#include "renderer/pipeline_layout_manager.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "utils/asset_manager.h"
#include "utils/buffer_utils.h"
#include "utils/image_utils.h"
#include "utils/vulkan_ext_funcs.h"

#include <algorithm>
#include <chrono>

const uint32_t gWidth = 800;
const uint32_t gHeight = 600;
const int gMaxFramesInFlight = 2;

const std::vector<const char*> gValidationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> gDeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
};

#ifdef NDEBUG
const bool gEnableValidationLayers = false;
#else
const bool gEnableValidationLayers = true;
#endif

void Ssbo::Init(VkDeviceSize _size) {
	mBufferSize = _size;

	Utils::BufferUtils::CreateBuffer(
		_size,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		mBuffer,
		mDeviceMemory
	);

	void* mapping = nullptr;
	vkMapMemory(App::GetInstance().GetLogicalDevice(), mDeviceMemory, 0, VK_WHOLE_SIZE, 0, &mapping);
	mPersistentMapping = mapping;
}

void Ssbo::Reset() {
	const VkDevice& logicalDevice = App::GetInstance().GetLogicalDevice();

	vkUnmapMemory(logicalDevice, mDeviceMemory);
	mPersistentMapping = nullptr;

	vkDestroyBuffer(logicalDevice, mBuffer, nullptr);
	vkFreeMemory(logicalDevice, mDeviceMemory, nullptr);

}

void Ssbo::WriteToDescriptorSet(const VkDescriptorSet& _set, uint32_t _binding) {
	VkDescriptorBufferInfo bufferInfo{
		.buffer = mBuffer,
		.offset = 0,
		.range = mBufferSize
	};

	VkWriteDescriptorSet descriptorWrite{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = _set,
		.dstBinding = _binding,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pBufferInfo = &bufferInfo
	};

	vkUpdateDescriptorSets(
		App::GetInstance().GetLogicalDevice(),

		// Write array
		1,
		&descriptorWrite,

		// Copy array
		0,
		nullptr
	);
}

static void FramebufferResizeCallback(GLFWwindow* /*_window*/, int /*_width*/, int /*_height*/) {
	App::GetInstance().framebufferResized = true;
}

static void MouseButtonCallback(GLFWwindow* /*_window*/, int _button, int _action, int /*_mods*/) {
	if (_button == GLFW_MOUSE_BUTTON_LEFT && _action == GLFW_PRESS) {
		AssetDefs::DenseId& texId = App::GetInstance().GetMaterial()->params.denseTexIds[0];
		texId = (texId + 1) % 3;

		if (texId == 0) { // now that we have a default texture, skip it
			texId++;
		}
	}

	if (_button == GLFW_MOUSE_BUTTON_RIGHT && _action == GLFW_PRESS) {
		AssetDefs::DenseId& intVar = App::GetInstance().GetMaterial()->params.denseTexIds[1];
		intVar = (intVar + 1) % 6;
	}

	if (_button == GLFW_MOUSE_BUTTON_MIDDLE && _action == GLFW_PRESS) {
		AssetManager::GetInstance().UnloadTexture("textures\\statue.jpg");
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
	glfwSetFramebufferSizeCallback(mWindow, FramebufferResizeCallback);
	glfwSetMouseButtonCallback(mWindow, MouseButtonCallback);
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
	
	Shader* shader = new Shader(
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

	CreateUniformBuffers();

	CreateDescriptorPools();
	CreateFrameDescriptorSets();
	CreateMaterialBuffers();
	CreateMaterialDescriptorSet();

	CreateCommandBuffers();
	CreateSyncObjects();

	// Create/load all our textures
	// IT DOES NOT MATTER THE LOAD ORDER ANYMORE, THE STABLE ID => DENSE ID WORKS! :)
	AssetManager::Init();
	AssetManager::GetInstance().onTextureLoaded = std::bind(&App::OnTextureLoaded, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	AssetManager::GetInstance().onTextureUnloaded = std::bind(&App::OnTextureUnloaded, this, std::placeholders::_1);
	AssetManager::GetInstance().LoadTexture("assets\\textures\\default_texture.png");
	AssetManager::GetInstance().LoadTexture("assets\\textures\\viking_room.png");
	AssetManager::GetInstance().LoadTexture("assets\\textures\\statue.jpg");

	// Initialize material with viking texture and default params
	mTempMaterial = new Material(
		shader,
		MaterialParams{
			.denseTexIds = { AssetManager::GetInstance().GetDenseIdForTexture("textures\\viking_room.png"), 0},
			.colorVars = {
				glm::vec4(1.f, 1.f, 1.f, 1.f), // WHITE
				glm::vec4(.05f, .05f, .05f, 1.f), // BLACK / DARK-GRAY
				glm::vec4(0.f, 1.f, 0.f, 1.f), // GREEN
				glm::vec4(1.f, 1.f, 0.f, 1.f), // YELLOW
				glm::vec4(0.f, 1.f, 1.f, 1.f), // CYAN
				glm::vec4(1.f, 0.f, 1.f, 1.f), // MAGENTA
			}
		}
	);

	mTempMesh = new Mesh("assets/models/viking_room.obj");
	mTempMesh->SetMaterial(mTempMaterial);
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

	// Enable some features that are needed for bindless indexing
	VkPhysicalDeviceDescriptorIndexingFeatures featureDescriptorIndexing{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,

		// Enable non-uniform array indexing
		.shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
		.shaderStorageImageArrayNonUniformIndexing = VK_TRUE,
		.shaderStorageTexelBufferArrayNonUniformIndexing = VK_TRUE,

		// These enable us to update after the 
		// commandBuffer has used 'vkBindDescriptorSets'
		.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
		.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
		.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,

		// Enable non-bound descriptor slots
		.descriptorBindingPartiallyBound = VK_TRUE,

		// Enable non-sized arrays
		.runtimeDescriptorArray = VK_TRUE,
	};

	VkDeviceCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,

		// Use descriptor indexing features
		.pNext = &featureDescriptorIndexing,

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

void App::CreateImageViews() {
	mSwapChainImageViews.resize(mSwapChainImages.size());

	for (uint32_t i = 0; i < mSwapChainImages.size(); i++) {
		mSwapChainImageViews[i] = Utils::ImageUtils::CreateImageView(mSwapChainImages[i], mSwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
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

	Utils::ImageUtils::CreateImage(
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

	mColorImageView = Utils::ImageUtils::CreateImageView(mColorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
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

	Utils::ImageUtils::CreateImage(
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

	mDepthImageView = Utils::ImageUtils::CreateImageView(mDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
}

void App::CreateUniformBuffers() {
	VkDeviceSize bufferSize = sizeof(CameraData);

	mUniformBuffers.resize(gMaxFramesInFlight);
	mUniformBuffersMemory.resize(gMaxFramesInFlight);
	mUniformBuffersMapped.resize(gMaxFramesInFlight);

	for (size_t i = 0; i < gMaxFramesInFlight; i++) {
		Utils::BufferUtils::CreateBuffer(
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			mUniformBuffers[i],
			mUniformBuffersMemory[i]
		);

		vkMapMemory(mLogicalDevice, mUniformBuffersMemory[i], 0, bufferSize, 0, &mUniformBuffersMapped[i]);
	}
}

void App::CreateDescriptorPools() {
	// ----- Frame descriptor pool -----
	// What type of descriptors our sets will contain, and how many
	VkDescriptorPoolSize framePoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(gMaxFramesInFlight) };

	// We have already said how many descriptors are available,
	// but we also need to specify how many descriptor sets may
	// be allocated via '.maxSets'
	VkDescriptorPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = static_cast<uint32_t>(gMaxFramesInFlight),
		.poolSizeCount = 1,
		.pPoolSizes = &framePoolSize,
	};

	if (vkCreateDescriptorPool(mLogicalDevice, &poolInfo, nullptr, &mFrameDescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create frame descriptor pool!");
	}

	// ----- Material descriptor pool -----
	std::array<VkDescriptorPoolSize, 2> materialPoolSizes = {
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, AssetManagerGlobals::gMaxTexCount }, // texSampler[]
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }, // runtimeToTexIndex[]
	};

	poolInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,

		// Allow updating sets after 'vkBindDescriptorSets'
		.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,

		.maxSets = 1,
		.poolSizeCount = static_cast<uint32_t>(materialPoolSizes.size()),
		.pPoolSizes = materialPoolSizes.data(),
	};

	if (vkCreateDescriptorPool(mLogicalDevice, &poolInfo, nullptr, &mMaterialDescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create material descriptor pool!");
	}
}

void App::CreateFrameDescriptorSets() {
	const VkDescriptorSetLayout& layoutPreset = PipelineLayoutManager::GetInstance().GetDescriptorSetLayout(DescriptorSet::Global);
	std::vector<VkDescriptorSetLayout> layouts(gMaxFramesInFlight, layoutPreset);

	VkDescriptorSetAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = mFrameDescriptorPool,
		.descriptorSetCount = static_cast<uint32_t>(gMaxFramesInFlight),
		.pSetLayouts = layouts.data()
	};

	mFrameDescriptorSets.resize(gMaxFramesInFlight);
	if (vkAllocateDescriptorSets(mLogicalDevice, &allocInfo, mFrameDescriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate frame descriptor sets!");
	}

	for (size_t i = 0; i < gMaxFramesInFlight; i++) {
		// Which buffer? And where the region within it
		// that contains the data for the descriptor
		VkDescriptorBufferInfo bufferInfo{
			.buffer = mUniformBuffers[i],
			.offset = 0,
			.range = sizeof(CameraData)
		};

		VkWriteDescriptorSet descriptorWrite{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = mFrameDescriptorSets[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &bufferInfo
		};

		// Apply updates - this accepts write arrays, and copy arrays
		vkUpdateDescriptorSets(
			mLogicalDevice, 

			// Write array
			1, 
			&descriptorWrite, 

			// Copy array
			0, 
			nullptr
		);
	}
}

void App::CreateMaterialBuffers() {
	mDenseIdToTextureSlot.Init(sizeof(AssetDefs::TextureSlot) * AssetManagerGlobals::gMaxTexCount);
}

void App::CreateMaterialDescriptorSet() {
	const VkDescriptorSetLayout& layoutPreset = PipelineLayoutManager::GetInstance().GetDescriptorSetLayout(DescriptorSet::Material);

	VkDescriptorSetAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = mMaterialDescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layoutPreset
	};

	if (vkAllocateDescriptorSets(mLogicalDevice, &allocInfo, &mMaterialDescriptorSet) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate material descriptor set!");
	}

	// Initial SSBO mappings
	mDenseIdToTextureSlot.WriteToDescriptorSet(mMaterialDescriptorSet, 1);
}

void App::OnTextureLoaded(const Texture& _tex, AssetDefs::DenseId _denseId, AssetDefs::TextureSlot _texSlot) {
	mDenseIdToTextureSlot.GetElement<AssetDefs::TextureSlot>(_denseId) = _texSlot;

	// We need a new texture sampler for this texture index
	if (!mTextureSlotToSampler.contains(_texSlot)) {
		CreateTextureSamplerForSlot(_texSlot);
	}

	VkDescriptorImageInfo imageInfo{
		.sampler = mTextureSlotToSampler[_texSlot],
		.imageView = _tex.GetImageView(),
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkWriteDescriptorSet descriptorWrite{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = mMaterialDescriptorSet,
		.dstBinding = 0,
		.dstArrayElement = _texSlot,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &imageInfo
	};

	// Apply updates - this accepts write arrays, and copy arrays
	vkUpdateDescriptorSets(
		mLogicalDevice,

		// Write array
		1,
		&descriptorWrite,

		// Copy array
		0,
		nullptr
	);
}

void App::OnTextureUnloaded(AssetDefs::DenseId _denseId) {
	// Point dense id at 'default_tex', signalling it is 'unloaded'
	mDenseIdToTextureSlot.GetElement<AssetDefs::TextureSlot>(_denseId) = 0;
}

void App::CreateTextureSamplerForSlot(AssetDefs::TextureSlot _texSlot) {
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
	vkGetPhysicalDeviceProperties(App::GetInstance().GetPhysicalDevice(), &properties);

	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

	mTextureSlotToSampler[_texSlot] = {};
	if (vkCreateSampler(App::GetInstance().GetLogicalDevice(), &samplerInfo, nullptr, &mTextureSlotToSampler[_texSlot]) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create texture sampler!");
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
		std::array<VkDescriptorSet, 2> sets = { mFrameDescriptorSets[mCurrentFrame], mMaterialDescriptorSet };

		vkCmdBindDescriptorSets(
			_commandBuffer, 
			VK_PIPELINE_BIND_POINT_GRAPHICS, 
			layout, 
			0,										// first descriptor set
			static_cast<uint32_t>(sets.size()),		// number of descriptor sets
			sets.data(),							// array of descriptor sets
			0, nullptr								// no dynamic offsets
		);

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

	CameraData ubo = {};
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

	for (auto& [slot, texSampler] : mTextureSlotToSampler) {
		vkDestroySampler(mLogicalDevice, texSampler, nullptr);
	}

	AssetManager::Shutdown();

	mDenseIdToTextureSlot.Reset();

	for (size_t i = 0; i < gMaxFramesInFlight; i++) {
		vkDestroyBuffer(mLogicalDevice, mUniformBuffers[i], nullptr);
		vkFreeMemory(mLogicalDevice, mUniformBuffersMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(mLogicalDevice, mFrameDescriptorPool, nullptr);
	vkDestroyDescriptorPool(mLogicalDevice, mMaterialDescriptorPool, nullptr);

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
	delete mTempMaterial;

	vkDestroyDevice(mLogicalDevice, nullptr);

	if (gEnableValidationLayers) {
		Utils::VulkanExtFuncs::DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
	}

	vkDestroySurfaceKHR(mInstance, mWindowSurface, nullptr);
	vkDestroyInstance(mInstance, nullptr);

	glfwDestroyWindow(mWindow);
	glfwTerminate();
}
