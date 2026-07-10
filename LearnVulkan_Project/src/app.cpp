#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "pch.h"

#include "app.h"
#include "renderer/material.h"
#include "renderer/mesh.h"
#include "renderer/pipeline_layout_manager.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/asset_manager.h"
#include "utils/debug_logger.h"
#include "utils/image_utils.h"
#include "utils/vulkan_ext_funcs.h"

#include <algorithm>
#include <chrono>

const glm::ivec2 gWindowExtents{ 800, 600 };
const int8_t gMaxFramesInFlight = 2;

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

void App::Run() {
	InitWindow();
	SetupInput();
	InitVulkan();

	Start();
	MainLoop();
}

void App::InitWindow() {
	mWindow = std::make_unique<Window>("Learn Vulkan", gWindowExtents);
	assert(mWindow);
	
	using namespace std::placeholders;

	mWindow->onWindowResized = [](glm::vec2) { App::GetInstance().mFramebufferResized = true; };
}

void App::SetupInput() {
	InputManager& inputInst = InputManager::GetInstance();

	inputInst.BindToWindow(*mWindow); // this binds the input manager to window input events

	// Move inputs
	{
		inputInst.AddInputEvent(Input::W, InputState::HOLD, 1.f, MAKE_CB_1(App::Event_MoveForward, this));
		inputInst.AddInputEvent(Input::S, InputState::HOLD, -1.f, MAKE_CB_1(App::Event_MoveForward, this));
		inputInst.AddInputEvent(Input::D, InputState::HOLD, 1.f, MAKE_CB_1(App::Event_MoveRight, this));
		inputInst.AddInputEvent(Input::A, InputState::HOLD, -1.f, MAKE_CB_1(App::Event_MoveRight, this));
		inputInst.AddInputEvent(Input::SPACE, InputState::HOLD, 1.f, MAKE_CB_1(App::Event_MoveUp, this));
		inputInst.AddInputEvent(Input::LEFT_CONTROL, InputState::HOLD, -1.f, MAKE_CB_1(App::Event_MoveUp, this));
	}
	
	// Look inputs
	{
		inputInst.AddInputEvent(Input::L, InputState::PRESS, 1.f, MAKE_CB_1(App::Event_LockCursor, this));
		inputInst.AddInputEvent(Input::U, InputState::PRESS, -1.f, MAKE_CB_1(App::Event_LockCursor, this));
		inputInst.AddInputEvent(Input::R, InputState::PRESS, MAKE_CB(App::Event_LookAtOrigin, this));

		inputInst.AddInputEvent(Input::MOUSE_MOVE, InputState::MOUSE_AXIS, glm::vec2(0.1f), MAKE_CB_1(App::Event_MouseMove, this));
		inputInst.AddInputEvent(Input::MOUSE_SCROLL, InputState::MOUSE_AXIS, glm::vec2(1), MAKE_CB_1(App::Event_MouseScroll, this));
	}

	// Mouse buttons
	{
		inputInst.AddInputEvent(Input::MOUSE_BUTTON_LEFT, InputState::PRESS, MAKE_CB(App::Event_IterateTextures, this));
		inputInst.AddInputEvent(Input::MOUSE_BUTTON_RIGHT, InputState::PRESS, MAKE_CB(App::Event_IterateColors, this));
		inputInst.AddInputEvent(Input::MOUSE_BUTTON_MIDDLE, InputState::PRESS, MAKE_CB(App::Event_ToggleTextureLoad, this));
	}
}

void App::Event_MoveForward(float _scale) {
	mCamera->AddMoveInput(_scale, mCamera->transform.GetForwardVector());
}

void App::Event_MoveRight(float _scale) {
	mCamera->AddMoveInput(_scale, mCamera->transform.GetRightVector());
}

void App::Event_MoveUp(float _scale) {
	mCamera->AddMoveInput(_scale, gWorldUp);
}

void App::Event_LockCursor(float _scale) {
	mWindow->SetMouseInputMode(_scale > 0.f ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void App::Event_LookAtOrigin() {
	mCamera->transform.LookAt({ 0, 0, 0 }, true);
}

void App::Event_MouseMove(const glm::vec2& _scale) {
	// Only allow look input if cursor is locked to window
	if (mWindow->GetMouseInputMode() != GLFW_CURSOR_NORMAL) {
		mCamera->AddYawInput(_scale.x);
		mCamera->AddPitchInput(_scale.y);
	}
}

void App::Event_MouseScroll(const glm::vec2& _scale) {
	float flySpeed = (mCamera->flySpeed + (_scale.y * 0.5f));
	mCamera->flySpeed = glm::clamp(flySpeed, 0.5f, 5.f);
}

void App::Event_IterateTextures() {
	if (auto testMaterial = AssetManager::GetInstance().GetAsset<Material>("materials\\test.material").lock()) {
		// Grab params* from material asset, and directly access its elements
		AssetDefs::DenseId& texId = testMaterial->params->denseTexIds[0];
		texId = (texId + 1) % 3;

		if (texId == 0) { // now that we have a default texture, skip it
			texId++;
		}
	}
}

void App::Event_IterateColors() {
	if (auto testMaterial = AssetManager::GetInstance().GetAsset<Material>("materials\\test.material").lock()) {
		// Grab params* from material asset, and directly access its elements
		int32_t& intVar = testMaterial->params->intVars[0];
		intVar = (intVar + 1) % 6;
	}
}

void App::Event_ToggleTextureLoad() {
	AssetManager& managerInst = AssetManager::GetInstance();

	const std::string texPath = "textures\\statue.jpg";

	if (managerInst.IsAssetLoaded<Texture>(texPath)) {
		managerInst.UnloadAsset<Texture>(texPath);
	}
	else {
		managerInst.LoadAsset<Texture>(std::filesystem::path("assets") / texPath);
	}
}

void App::InitVulkan() {
	CreateInstance();
	SetupDebugMessenger();
	
	mWindow->CreateWindowSurface(mInstance);

	PickPhysicalDevice();
	CreateLogicalDevice();

	PipelineLayoutManager::Init();

	mSwapchain = std::make_unique<Swapchain>();
	assert(mSwapchain);

	// Create swapchain, and time creation of other resources
	mSwapchain->CreateSwapchain();
	{
		CreateRenderPass();
		CreateColorResources();
		CreateDepthResources();
	}
	mSwapchain->CreateImageViews();
	mSwapchain->CreateFramebuffers(mColorTexture.GetImageView(), mDepthTexture.GetImageView());
	mSwapchain->CreateSyncObjects();

	CreateCommandPool();

	// Init per-frame data
	mFrameData.resize(gMaxFramesInFlight);
	for (FrameData& frame : mFrameData) {
		frame.Init(mLogicalDevice, mCommandPool);
	}

	// Init bindless material system data
	CreateMaterialDescriptorPool();
	mMaterialData.Init(mLogicalDevice, mMaterialDescriptorPool);

	// Create/load all our textures
	// IT DOES NOT MATTER THE LOAD ORDER ANYMORE, THE STABLE ID => DENSE ID WORKS! :)
	using namespace std::placeholders;

	AssetManager::Init();
	AssetManager::GetInstance().GetLoadCallback<Texture>() = std::bind(&MaterialData::OnTextureLoaded, &mMaterialData, _1, _2, _3);
	AssetManager::GetInstance().GetUnloadCallback<Texture>() = std::bind(&MaterialData::OnTextureUnloaded, &mMaterialData, _1);
	AssetManager::GetInstance().GetLoadCallback<Material>() = std::bind(&MaterialData::OnMaterialLoaded, &mMaterialData, _1, _2, _3);
	AssetManager::GetInstance().GetUnloadCallback<Material>() = std::bind(&MaterialData::OnMaterialUnloaded, &mMaterialData, _1);

	AssetManager::GetInstance().LoadAsset<Texture>("assets\\textures\\default_texture.png");
	AssetManager::GetInstance().LoadAsset<Texture>("assets\\textures\\viking_room.png");
	AssetManager::GetInstance().LoadAsset<Texture>("assets\\textures\\statue.jpg");

	auto testMat = AssetManager::GetInstance().LoadAsset<Material>("assets\\materials\\test.material");

	mTempMesh = AssetManager::GetInstance().LoadAsset<Mesh>("assets\\models\\viking_room.mesh");
	if (auto meshLock = mTempMesh.lock()) {
		meshLock->SetMaterial(testMat);
	}
}

void App::CreateInstance() {
	LOG_MSG("Creating Vulkan instance", LogVerbosity::Info);

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
	LOG_MSG("Finding suitable physical device", LogVerbosity::Info);

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
		SwapchainSupportDetails swapChainSupport = mSwapchain->QuerySwapChainSupport(_device, mWindow->GetWindowSurface());
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
		vkGetPhysicalDeviceSurfaceSupportKHR(_device, i, mWindow->GetWindowSurface(), &presentSupport);

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
	LOG_MSG("Creating logical device", LogVerbosity::Info);

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

void App::CreateRenderPass() {
	VkAttachmentDescription colorAttachment = {
		.format = mSwapchain->GetFormat(),
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
		.format = mSwapchain->GetFormat(),
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
	mColorTexture.CreateImage(
		mSwapchain->GetExtent(),
		mMsaaState.samples,
		mSwapchain->GetFormat(),
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);
}

VkFormat App::FindDepthFormat() {
	return FindSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

void App::CreateDepthResources() {
	mDepthTexture.CreateImage(
		mSwapchain->GetExtent(),
		mMsaaState.samples,
		FindDepthFormat(),
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT
	);
}

void App::CreateMaterialDescriptorPool() {
	LOG_MSG("Creating material descriptor pool", LogVerbosity::Info);

	// ----- Material descriptor pool -----
	std::array<VkDescriptorPoolSize, 2> materialPoolSizes = {
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, AssetManagerGlobals::AssetTraits<Texture>::config.maxCount }, // texSampler[]
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 }, // denseIdToTexSlot[], materialParams[], denseIdToMatSlot[]
	};

	VkDescriptorPoolCreateInfo poolInfo = {
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
		.framebuffer = mSwapchain->GetFramebuffer(_imageIndex),
		.renderArea = {
			.offset = { 0, 0 },
			.extent = mSwapchain->GetExtent()
		},
		.clearValueCount = static_cast<uint32_t>(clearValues.size()),
		.pClearValues = clearValues.data()
	};

	// -- Render Pass --
	vkCmdBeginRenderPass(_commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	{
		auto lockedMesh = mTempMesh.lock();

		// This is where the shader (pipeline) and vertex/index buffers get bound for this frame
		if (lockedMesh) lockedMesh->BindMeshResources(_commandBuffer);

		// Bind descriptor sets - this is for uniform buffers

		// Eventually, this will only be called at the beginning of each sub-pass,
		// where we also will switch pipeline layout
		const VkPipelineLayout& layout = PipelineLayoutManager::GetInstance().GetLayoutForModel(BlendModel::Opaque, ShadingModel::Unlit);
		std::array<VkDescriptorSet, 2> sets = { mFrameData[mCurrentFrame].GetDescriptorSet(), mMaterialData.GetDescriptorSet() };

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
		const VkExtent2D& swapchainExtent = mSwapchain->GetExtent();
		VkViewport viewport = {
			.x = 0.f,
			.y = 0.f,
			.width = static_cast<float>(swapchainExtent.width),
			.height = static_cast<float>(swapchainExtent.height),
			.minDepth = 0.f,
			.maxDepth = 1.f
		};
		vkCmdSetViewport(_commandBuffer, 0, 1, &viewport);

		VkRect2D scissor = {
			.offset = { 0, 0 },
			.extent = swapchainExtent
		};
		vkCmdSetScissor(_commandBuffer, 0, 1, &scissor);

		// This is where the actual draw call happens for our mesh
		if (lockedMesh) lockedMesh->DrawMesh(_commandBuffer);
	}
	vkCmdEndRenderPass(_commandBuffer);

	if (vkEndCommandBuffer(_commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to record command buffer!");
	}
}

void App::RecreateSwapchain() {
	LOG_MSG("Re-creating swapchain", LogVerbosity::Info);

	// If GLFW window is minimized, the framebuffer has size 0,
	// for now we just pause rendering until the size is non-zero
	const glm::ivec2& windowExtents = mWindow->GetExtents();
	while (windowExtents.x == 0 || windowExtents.y == 0) {
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(mLogicalDevice);

	CleanupSwapchain();

	// Re-create swapchain, and time creation of other resources
	mSwapchain->CreateSwapchain();
	{
		CreateColorResources();
		CreateDepthResources();
	}
	mSwapchain->CreateImageViews();
	mSwapchain->CreateFramebuffers(mColorTexture.GetImageView(), mDepthTexture.GetImageView());
}

void App::CleanupSwapchain(bool _isFinalCleanup) {
	LOG_MSG("Cleaning up swapchain", LogVerbosity::Info);

	mColorTexture.CleanupImage();
	mDepthTexture.CleanupImage();

	mSwapchain->CleanupSwapchain();

	if (_isFinalCleanup) {
		mSwapchain->CleanupSyncObjects();
	}
}

void App::Start() {
	mCamera = std::make_unique<FlyCamera>(
		45.f,			// fov
		0.1f, 10.f,		// near-far clip
		3.f,			// fly speed
		1.5f			// look speed
	);

	const VkExtent2D& swapchainExtent = mSwapchain->GetExtent();
	mCamera->SetViewSize({ swapchainExtent.width, swapchainExtent.height });
	mCamera->transform.SetPosition({ 3, 3, 3 });
	mCamera->transform.LookAt({ 0, 0, 0 });
}

void App::MainLoop() {
	float deltaTime = 0.f;

	while (!mWindow->GetShouldClose()) {
		auto frameStart = std::chrono::high_resolution_clock::now();

		// Core app loop
		{
			glfwPollEvents();

			InputManager::GetInstance().DispatchEvents();
			mCamera->Update(deltaTime);

			DrawFrame(deltaTime);
		}

		auto frameEnd = std::chrono::high_resolution_clock::now();
		deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(frameEnd - frameStart).count();
	}

	// Wait for the logical device to finish operations
	vkDeviceWaitIdle(mLogicalDevice);
}

void App::DrawFrame(float _deltaTime) {
	const FrameData& currentFrameData = mFrameData[mCurrentFrame];
	const VkFence& inFlightFence = currentFrameData.GetInFlightFence();
	const VkSemaphore& imageAvailableSemaphore = currentFrameData.GetImageAvailableSemaphore();
	const VkCommandBuffer& commandBuffer = currentFrameData.GetCommandBuffer();

	// Wait for all fences in an array, for uint64_t::max timeout
	vkWaitForFences(mLogicalDevice, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

	// Acquire image from swapchain, and only signal semaphore on completion
	uint32_t imageIndex = 0;
	VkResult result = mSwapchain->AcquireNextImage(imageAvailableSemaphore, imageIndex);

	// Swapchain is out of date and must be recreated,
	// i.e window has been resized
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		RecreateSwapchain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("Failed to acquire swapchain image!");
	}

	// Manually reset our fences, only if we are submitting work
	vkResetFences(mLogicalDevice, 1, &inFlightFence);

	UpdateUniformBuffer(mCurrentFrame, _deltaTime);

	// Reset + record our command buffer
	vkResetCommandBuffer(commandBuffer, 0);
	RecordCommandBuffer(commandBuffer, imageIndex);

	VkSemaphore waitSemaphores[] = {
		imageAvailableSemaphore
	};

	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	VkSemaphore signalSemaphores[] = {
		// Use actual swapchain image index for rendering semaphore
		mSwapchain->GetRenderFinishedSemaphore(imageIndex)
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
		.pCommandBuffers = &commandBuffer,

		// Which semaphores to signal when command buffers
		// have finished their execution
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = signalSemaphores,
	};

	// Submit drawing commands, and signal our fence when done
	if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("Failed to submit draw command buffer!");
	}

	VkSwapchainKHR swapChains[] = {
		mSwapchain->GetSwapchain()
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
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || mFramebufferResized) {
		mFramebufferResized = false;
		RecreateSwapchain();

		// Update camera view size if frame buffer is resized
		const VkExtent2D& swapchainExtent = mSwapchain->GetExtent();
		mCamera->SetViewSize({ swapchainExtent.width, swapchainExtent.height });
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to present swap chain image!");
	}

	// Advance to next frame
	mCurrentFrame = (mCurrentFrame + 1) % gMaxFramesInFlight;
}

void App::UpdateUniformBuffer(uint32_t _currentImage, float _deltaTime) {
	// TODO - move this somewhere else now that mesh has its' own transform (matrix)
	if (auto lockedMesh = mTempMesh.lock()) {
		lockedMesh->transform.AddRotation(glm::angleAxis(
			_deltaTime * glm::radians(90.f),	// rotation (in radians)
			gWorldUp							// axis of rotation
		));
	}

	assert(mCamera);

	// Copy ubo directly into already mapped buffer
	const CameraData& camData = mCamera->GetGraphicsData();
	void* mapped = mFrameData[_currentImage].GetUniformBuffer().mapped;
	memcpy(mapped, &camData, sizeof(CameraData));
}

void App::OnInitialized() {
	LOG_MSG("Init", LogVerbosity::Info);
}

void App::OnCleanup() {
	// Ensure everything has finished. This is only really necessary if the
	// program is ended via a thrown exception, main loop exit does this already
	{
		mWindow->SetShouldClose();
		vkDeviceWaitIdle(mLogicalDevice);
	}

	CleanupSwapchain(true);

	AssetManager::Shutdown();

	mMaterialData.Reset();
	mFrameData.clear();

	vkDestroyDescriptorPool(mLogicalDevice, mMaterialDescriptorPool, nullptr);

	for (size_t i = 0; i < mRenderPasses.size(); i++) {
		vkDestroyRenderPass(mLogicalDevice, mRenderPasses[i], nullptr);
	}

	vkDestroyCommandPool(mLogicalDevice, mCommandPool, nullptr);

	PipelineLayoutManager::Shutdown();

	vkDestroyDevice(mLogicalDevice, nullptr);

	if (gEnableValidationLayers) {
		Utils::VulkanExtFuncs::DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
	}

	LOG_MSG("Cleanup", LogVerbosity::Info);
}
