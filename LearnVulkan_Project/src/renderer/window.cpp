#include "pch.h"
#include "renderer/window.h"

#include "app.h"
#include "utils/debug_logger.h"

Window::Window(const char* _title, const glm::ivec2& _extents) {
	LOG_MSG("Creating window & initializing glfw", LogVerbosity::Info);

	glfwInit();

	// Since GLFW was designed to create an OpenGL context,
	// we need to tell it to *not* create one
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// 1,2,3 params are window width, height and title
	// 4th param => specifies which monitor to open window on
	// 5th param => specific to OpenGL
	mWindow = glfwCreateWindow(_extents.x, _extents.y, _title, nullptr, nullptr);
	mExtents = _extents;

	// Set GLFW window to point to us, so we can use it in callbacks
	glfwSetWindowUserPointer(mWindow, this);

	SetupCallbacks();
}

Window::~Window() {
	LOG_MSG("Destroying window & terminating glfw", LogVerbosity::Info);

	const VkInstance& vulkanInst = App::GetInstance().GetVulkanInstance();

	vkDestroySurfaceKHR(vulkanInst, mSurface, nullptr);
	vkDestroyInstance(vulkanInst, nullptr);

	glfwDestroyWindow(mWindow);
	glfwTerminate();
}

void Window::CreateWindowSurface(const VkInstance& _instance) {
	if (glfwCreateWindowSurface(_instance, mWindow, nullptr, &mSurface) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create window surface!");
	}
}

void Window::SetupCallbacks() {
	// Since these lambdas are defined within 'Window', they can
	// access private variables / functions... very nice :)

	glfwSetFramebufferSizeCallback(mWindow, [](GLFWwindow* _window, int _width, int _height) {
		if (Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(_window))) {
			windowPtr->mExtents = { _width, _height };

			if (windowPtr->onWindowResized) {
				windowPtr->onWindowResized(glm::ivec2(_width, _height));
			}
		}
	});

	glfwSetMouseButtonCallback(mWindow, [](GLFWwindow* _window, int _button, int _action, int) {
		if (Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(_window))) {
			if (windowPtr->onMouseClicked) {
				windowPtr->onMouseClicked(_button, _action);
			}
		}
	});

	glfwSetCursorPosCallback(mWindow, [](GLFWwindow* _window, double _xPos, double _yPos) {
		static glm::vec2 lastMousePos{ _xPos, _yPos };
		const glm::vec2 currentMousePos{ _xPos, _yPos };

		if (Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(_window))) {
			if (windowPtr->onMouseMoved) {
				// Subtraction is flipped as we must negate both axes for it to be sensible to use
				windowPtr->onMouseMoved(lastMousePos - currentMousePos);
			}
		}

		lastMousePos = { _xPos, _yPos };
	});

	glfwSetScrollCallback(mWindow, [](GLFWwindow* _window, double _xDelta, double _yDelta) {
		if (Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(_window))) {
			if (windowPtr->onMouseScrolled) {
				windowPtr->onMouseScrolled(glm::vec2(_xDelta, _yDelta));
			}
		}
	});

	glfwSetKeyCallback(mWindow, [](GLFWwindow* _window, int _key, int, int _action, int) {
		if (Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(_window))) {
			if (windowPtr->onKeyInput) {
				windowPtr->onKeyInput(_key, _action);
			}
		}
	});
}
