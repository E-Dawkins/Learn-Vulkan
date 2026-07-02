#pragma once
#include <GLFW/glfw3.h>
#include <functional>

namespace WindowCallbacks {
	// Callback params: window size
	typedef std::function<void(glm::ivec2)> OnWindowResized;

	// Callback params: mouse button, action
	typedef std::function<void(int, int)> OnMouseClicked;

	// Callback params: mouse position delta
	typedef std::function<void(glm::vec2)> OnMouseMoved;

	// Callback params: mouse scroll delta
	typedef std::function<void(glm::vec2)> OnMouseScrolled;

	// Callback params: key, action
	typedef std::function<void(int, int)> OnKeyInput;
}

class Window
{
protected:
	GLFWwindow* mWindow = nullptr;
	VkSurfaceKHR mSurface = nullptr;

	glm::ivec2 mExtents{};

public:
	WindowCallbacks::OnWindowResized onWindowResized;
	WindowCallbacks::OnMouseClicked onMouseClicked;
	WindowCallbacks::OnMouseMoved onMouseMoved;
	WindowCallbacks::OnMouseScrolled onMouseScrolled;
	WindowCallbacks::OnKeyInput onKeyInput;

public:
	Window(const char* _title, const glm::ivec2& _extents);
	~Window();

	void CreateWindowSurface(const VkInstance& _instance);
	inline const VkSurfaceKHR& GetWindowSurface() const { return mSurface; }

	inline void SetMouseInputMode(int _mode) { glfwSetInputMode(mWindow, GLFW_CURSOR, _mode); }
	inline const int GetMouseInputMode() const { return glfwGetInputMode(mWindow, GLFW_CURSOR); }

	inline void SetExtents(const glm::ivec2& _extents) { glfwSetWindowSize(mWindow, _extents.x, _extents.y); }
	inline const glm::ivec2& GetExtents() const { return mExtents; }

	inline void SetShouldClose() { glfwSetWindowShouldClose(mWindow, GLFW_TRUE); }
	inline bool GetShouldClose() const { return glfwWindowShouldClose(mWindow); }

protected:
	void SetupCallbacks();
};