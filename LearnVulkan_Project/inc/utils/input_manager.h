#pragma once
#include "interfaces/singleton.h"

class Window;

// UINT16_MAX = unknown key
typedef uint16_t Key;

enum class KeyState : uint8_t
{
	RELEASE,
	PRESS,
	HOLD
};

struct KeyEvent
{
	typedef std::function<void(float)> Callback;
	
	Callback callback;
	float scale;
};

enum class MouseState : uint8_t
{
	MOVE,
	SCROLL
};

struct MouseEvent
{
	typedef std::function<void(const glm::vec2&)> Callback;

	Callback callback;
	glm::vec2 scale;
};

namespace p = std::placeholders;
#define MAKE_CB(func, obj) std::bind(&func, obj, p::_1)

class InputManager : public ISingleton<InputManager>
{
protected:
	std::unordered_map<Key, KeyState> mKeyStates;
	std::unordered_map<size_t, std::vector<KeyEvent>> mKeyEvents;

	std::unordered_map<size_t, std::vector<MouseEvent>> mMouseEvents;

	std::vector<size_t> mQueuedEvents;

	glm::vec2 mMouseScrollScale{ 1, 1 };
	glm::vec2 mMouseMoveScale{ 1, 1 };

public:
	void OnInitialized();
	void OnCleanup();

	void BindToWindow(Window& _window);

	void AddKeyEvent(Key _key, KeyState _state, float _scale, KeyEvent::Callback _callback);
	void AddMouseEvent(MouseState _state, glm::vec2 _scale, MouseEvent::Callback _callback);

	void DispatchEvents();

protected:
	constexpr size_t GetKeyEventHash(Key _key, KeyState _state) const;
	void DeconstructKeyEventHash(size_t _hash, Key& _outKey, KeyState& _outState);
	constexpr KeyState GetKeyStateFromAction(int _action) const;
	bool DispatchKeyEvent(size_t _eventHash);

	constexpr size_t GetMouseEventHash(MouseState _state) const;
	bool DispatchMouseEvent(size_t _eventHash);

	void ProcessKeyInput(int _key, int _action);
	void ProcessMouseMovement(const glm::vec2& _deltaPos);
	void ProcessMouseScroll(const glm::vec2& _scrollDelta);
};