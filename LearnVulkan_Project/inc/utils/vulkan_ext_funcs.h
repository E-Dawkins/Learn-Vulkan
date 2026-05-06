#pragma once

#include <vulkan/vulkan.h>

namespace Utils {
	namespace VulkanExtFuncs {
		VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT _messageSeverity, VkDebugUtilsMessageTypeFlagsEXT _messageType, const VkDebugUtilsMessengerCallbackDataEXT* _callbackData, void* _userData);

		VkResult CreateDebugUtilsMessengerEXT(VkInstance _instance, const VkDebugUtilsMessengerCreateInfoEXT* _createInfo, const VkAllocationCallbacks* _allocator, VkDebugUtilsMessengerEXT* _debugMessenger);
		void DestroyDebugUtilsMessengerEXT(VkInstance _instance, VkDebugUtilsMessengerEXT _debugMessenger, const VkAllocationCallbacks* _allocator);
		VkResult SetDebugUtilsObjectNameEXT(VkInstance _instance, VkDevice _device, const VkDebugUtilsObjectNameInfoEXT* _nameInfo);
	}
}

