#include "pch.h"

#include "utils/vulkan_ext_funcs.h"

VKAPI_ATTR VkBool32 VKAPI_CALL Utils::VulkanExtFuncs::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT /*_messageSeverity*/, VkDebugUtilsMessageTypeFlagsEXT /*_messageType*/, const VkDebugUtilsMessengerCallbackDataEXT* _callbackData, void* /*_userData*/) {
	std::cerr << "Validation layer: " << _callbackData->pMessage << "\n";
	return false;
}

#define GET_FUNC(x) (PFN_ ## x)(vkGetInstanceProcAddr(_instance, #x))

VkResult Utils::VulkanExtFuncs::CreateDebugUtilsMessengerEXT(VkInstance _instance, const VkDebugUtilsMessengerCreateInfoEXT* _createInfo, const VkAllocationCallbacks* _allocator, VkDebugUtilsMessengerEXT* _debugMessenger) {
	auto func = GET_FUNC(vkCreateDebugUtilsMessengerEXT);
	if (func != nullptr) {
		return func(_instance, _createInfo, _allocator, _debugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void Utils::VulkanExtFuncs::DestroyDebugUtilsMessengerEXT(VkInstance _instance, VkDebugUtilsMessengerEXT _debugMessenger, const VkAllocationCallbacks* _allocator) {
	auto func = GET_FUNC(vkDestroyDebugUtilsMessengerEXT);
	if (func != nullptr) {
		func(_instance, _debugMessenger, _allocator);
	}
}

VkResult Utils::VulkanExtFuncs::SetDebugUtilsObjectNameEXT(VkInstance _instance, VkDevice _device, const VkDebugUtilsObjectNameInfoEXT* _nameInfo) {
	auto func = GET_FUNC(vkSetDebugUtilsObjectNameEXT);
	if (func != nullptr) {
		return func(_device, _nameInfo);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}
