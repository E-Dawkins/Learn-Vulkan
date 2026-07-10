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
	_window.onKeyInput = std::bind(&InputManager::ProcessButtonInput, this, _1, _2);
	_window.onMouseClicked = std::bind(&InputManager::ProcessButtonInput, this, _1, _2);

	// These are specialized cases
	_window.onMouseMoved = std::bind(&InputManager::ProcessMouseMovement, this, _1);
	_window.onMouseScrolled = std::bind(&InputManager::ProcessMouseScroll, this, _1);
}

void InputManager::AddInputEvent(Input _input, InputState _state, EventCallback<std::monostate> _callback) {
	const size_t eventHash = GetInputEventHash(_input, _state);
	mInputEvents[eventHash].push_back(InputManagerImpl::InputEvent<std::monostate>{
		.callback = _callback
	});
}

void InputManager::DispatchEvents() {
	for (size_t i = 0; i < mQueuedEvents.size();) {
		size_t eventHash = mQueuedEvents[i];

		bool eventHandled = false;

		if (mInputEvents.contains(eventHash)) {
			if (eventHandled = DispatchInputEvent(eventHash); eventHandled) {
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

constexpr size_t InputManager::GetInputEventHash(Input _input, InputState _state) const {
	// ---- ---- XXXX XXXX => key
	// XXXX XXXX ---- ---- => state
	// *not 100% accurate, but an idea of the packing

	return (size_t)_input | (size_t(_state) << 16);
}

void InputManager::DeconstructInputEventHash(size_t _hash, Input& _outInput, InputState& _outState) {
	_outInput = static_cast<Input>(_hash & 0xFFFF); // first 16 bits
	_outState = static_cast<InputState>(_hash >> 16); // the rest
}

constexpr InputState InputManager::GetStateFromAction(int _action) const {
	switch (_action) {
		case GLFW_PRESS: return InputState::PRESS;
		case GLFW_REPEAT: return InputState::HOLD;
		default: return InputState::RELEASE; // if undertermined, safest option is 'released'
	}
}

bool InputManager::DispatchInputEvent(size_t _eventHash) {
	Input button;
	InputState state;
	DeconstructInputEventHash(_eventHash, button, state);
	
	// Run all callback events
	for (const auto& event : mInputEvents[_eventHash]) {
		std::visit([&](auto& _e) {
			using T = typename std::remove_reference_t<decltype(_e)>::ValueType;

			if constexpr (std::is_same_v<T, void>) {
				_e.callback();
			}
			else {
				// Since 'mModifiers' and 'mInputEvents' share the same scale
				// types, we can guarantee that 'T' is valid in this context
				if (mModifiers.contains(_eventHash)) {
					using T = decltype(_e.scale);

					T modifier = std::get<T>(mModifiers[_eventHash]);
					_e.callback(_e.scale * modifier);
				}
				else {
					_e.callback(_e.scale);
				}
			}
		}, event);
	}

	// We have 'handled' this event if it is not a 'HOLD' event
	// or the old/new states are mismatched (i.e. HOLD -> RELEASED)
	return (state != InputState::HOLD)
		|| (mButtonStates[button] != state);
}

void InputManager::ProcessButtonInput(int _button, int _action) {
	const Input button = static_cast<Input>(_button);

	InputState& currentState = mButtonStates[button];
	InputState newState = GetStateFromAction(_action);

	if (currentState != newState) {
		// Queue event to be called later on
		size_t eventHash = GetInputEventHash(button, newState);
		mQueuedEvents.push_back(eventHash);

		// Update mapping
		currentState = newState;
	}
}

void InputManager::ProcessMouseMovement(const glm::vec2& _deltaPos) {
	size_t eventHash = GetInputEventHash(Input::MOUSE_MOVE, InputState::MOUSE_AXIS);

	mModifiers[eventHash] = _deltaPos;
	mQueuedEvents.push_back(eventHash);
}

void InputManager::ProcessMouseScroll(const glm::vec2& _scrollDelta) {
	size_t eventHash = GetInputEventHash(Input::MOUSE_SCROLL, InputState::MOUSE_AXIS);
	
	mModifiers[eventHash] = _scrollDelta;
	mQueuedEvents.push_back(eventHash);
}
