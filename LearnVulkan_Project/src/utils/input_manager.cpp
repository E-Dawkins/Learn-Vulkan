#include "pch.h"
#include "utils/input_manager.h"

#include "renderer/window.h"
#include "utils/debug_logger.h"

void InputManager::OnInitialized() {
	LOG_MSG("Init", LogVerbosity::Info);
}

void InputManager::OnCleanup() {
	LOG_MSG("Cleanup", LogVerbosity::Info);
}

void InputManager::BindToWindow(Window& _window) {
	using namespace std::placeholders;

	// Since GLFW keys are 32+, and mouse buttons 0-7, we can re-use the same logic
	_window.onKeyInput = std::bind(&InputManager::ProcessKeyInput, this, _1, _2);
	_window.onMouseClicked = std::bind(&InputManager::ProcessKeyInput, this, _1, _2);

	// These are specialized cases
	_window.onMouseMoved = std::bind(&InputManager::ProcessMouseMovement, this, _1);
	_window.onMouseScrolled = std::bind(&InputManager::ProcessMouseScroll, this, _1);
}

void InputManager::AddKeyEvent(Key _key, KeyState _state, float _scale, KeyEvent::Callback _callback) {
	const size_t eventHash = GetKeyEventHash(_key, _state);
	mKeyEvents[eventHash].push_back(KeyEvent{ .callback = _callback, .scale = _scale });
}

void InputManager::AddMouseEvent(MouseState _state, glm::vec2 _scale, MouseEvent::Callback _callback) {
	const size_t eventHash = GetMouseEventHash(_state);
	mMouseEvents[eventHash].push_back(MouseEvent{ .callback = _callback, .scale = _scale });
}

void InputManager::DispatchEvents() {
	for (size_t i = 0; i < mQueuedEvents.size();) {
		size_t eventHash = mQueuedEvents[i];

		bool eventHandled = false;

		if (mKeyEvents.contains(eventHash)) {
			if (eventHandled = DispatchKeyEvent(eventHash); eventHandled) {
				mQueuedEvents.erase(mQueuedEvents.begin() + i);
			}
		}
		else if (mMouseEvents.contains(eventHash)) {
			if (eventHandled = DispatchMouseEvent(eventHash); eventHandled) {
				mQueuedEvents.erase(mQueuedEvents.begin() + i);
			}
		}
		else {
			// If event can never be handled, ignore it, remove it
			mQueuedEvents.erase(mQueuedEvents.begin() + i);
		}

		// Events are usually un-handled if it is a continued 'HOLD' event
		if (!eventHandled) {
			i++;
		}
	}
}

constexpr size_t InputManager::GetKeyEventHash(Key _key, KeyState _state) const {
	// ---- ---- XXXX XXXX => key
	// XXXX XXXX ---- ---- => state
	// *not 100% accurate, but an idea of the packing

	return _key | (size_t(_state) << 16);
}

void InputManager::DeconstructKeyEventHash(size_t _hash, Key& _outKey, KeyState& _outState) {
	_outKey = _hash & 0xFFFF; // first 16 bits
	_outState = static_cast<KeyState>(_hash >> 16); // the rest
}

constexpr KeyState InputManager::GetKeyStateFromAction(int _action) const {
	switch (_action) {
		case GLFW_PRESS: return KeyState::PRESS;
		case GLFW_REPEAT: return KeyState::HOLD;
		default: return KeyState::RELEASE; // if undertermined, safest option is 'released'
	}
}

bool InputManager::DispatchKeyEvent(size_t _eventHash) {
	Key key;
	KeyState state;
	DeconstructKeyEventHash(_eventHash, key, state);

	// Run all callback events
	for (const auto& event : mKeyEvents[_eventHash]) {
		event.callback(event.scale);
	}

	// We have 'handled' this event if it is not a 'HOLD' event
	// or the old/new states are mismatched (i.e. HOLD -> RELEASED)
	return (state != KeyState::HOLD)
		|| (mKeyStates[key] != state);
}

constexpr size_t InputManager::GetMouseEventHash(MouseState _state) const {
	// Since GLFW keys start from 32+, and mouse buttons
	// range from 0-7, 10+ should be safe for custom events
	return 10 + size_t(_state);
}

bool InputManager::DispatchMouseEvent(size_t _eventHash) {
	bool isMoveEvent = (_eventHash == GetMouseEventHash(MouseState::MOVE));
	const glm::vec2 currentScale = (isMoveEvent ? mMouseMoveScale : mMouseScrollScale);

	// Run all callback events
	for (const auto& event : mMouseEvents[_eventHash]) {
		event.callback(event.scale * currentScale);
	}

	return true;
}

void InputManager::ProcessKeyInput(int _key, int _action) {
	const Key key = static_cast<Key>(_key);

	KeyState& currentState = mKeyStates[key];
	KeyState newState = GetKeyStateFromAction(_action);

	if (currentState != newState) {
		// Queue event to be called later on
		size_t eventHash = GetKeyEventHash(key, newState);
		mQueuedEvents.push_back(eventHash);

		// Update mapping
		currentState = newState;
	}
}

void InputManager::ProcessMouseMovement(const glm::vec2& _deltaPos) {
	mMouseMoveScale = _deltaPos;

	size_t eventHash = GetMouseEventHash(MouseState::MOVE);
	mQueuedEvents.push_back(eventHash);
}

void InputManager::ProcessMouseScroll(const glm::vec2& _scrollDelta) {
	mMouseScrollScale = _scrollDelta;
	
	size_t eventHash = GetMouseEventHash(MouseState::SCROLL);
	mQueuedEvents.push_back(eventHash);
}
