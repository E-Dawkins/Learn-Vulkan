#pragma once
#include "interfaces/singleton.h"

#include <variant>

class Window;

// This enum is setup to match GLFW button mapping + our own custom mappings
enum class Input : uint16_t
{
	// Mouse buttons
	MOUSE_BUTTON_LEFT = 0,
	MOUSE_BUTTON_RIGHT,
	MOUSE_BUTTON_MIDDLE,
	MOUSE_BUTTON_4,
	MOUSE_BUTTON_5,
	MOUSE_BUTTON_6,
	MOUSE_BUTTON_7,

	// Other mouse inputs
	MOUSE_MOVE,
	MOUSE_SCROLL,

	// Keyboard buttons - printable
	SPACE = 32,
	APOSTROPHE = 39,
	COMMA = 44,
	MINUS,
	PERIOD,
	FORWARD_SLASH,
	ZERO,
	ONE,
	TWO,
	THREE,
	FOUR,
	FIVE,
	SIX,
	SEVEN,
	EIGHT,
	NINE,
	SEMICOLON = 59,
	EQUAL = 61,
	A = 65,
	B,
	C,
	D,
	E,
	F,
	G,
	H,
	I,
	J,
	K,
	L,
	M,
	N,
	O,
	P,
	Q,
	R,
	S,
	T,
	U,
	V,
	W,
	X,
	Y,
	Z,
	LEFT_BRACKET,
	BACK_SLASH,
	RIGHT_BRACKET,
	GRAVE_ACCENT = 96,

	// Keyboard buttons - function
	ESCAPE = 256,
	ENTER,
	TAB,
	BACKSPACE,
	INSERT,
	DELETE,
	RIGHT,
	LEFT,
	DOWN,
	UP,
	PAGE_UP,
	PAGE_DOWN,
	HOME,
	END,
	CAPS_LOCK = 280,
	SCROLL_LOCK,
	NUM_LOCK,
	PRINT_SCREEN,
	PAUSE,
	F1 = 290,
	F2,
	F3,
	F4,
	F5,
	F6,
	F7,
	F8,
	F9,
	F10,
	F11,
	F12,
	F13,
	F14,
	F15,
	F16,
	F17,
	F18,
	F19,
	F20,
	F21,
	F22,
	F23,
	F24,
	F25,
	NUMPAD_ZERO = 320,
	NUMPAD_ONE,
	NUMPAD_TWO,
	NUMPAD_THREE,
	NUMPAD_FOUR,
	NUMPAD_FIVE,
	NUMPAD_SIX,
	NUMPAD_SEVEN,
	NUMPAD_EIGHT,
	NUMPAD_NINE,
	NUMPAD_DECIMAL,
	NUMPAD_DIVIDE,
	NUMPAD_MULTIPLY,
	NUMPAD_SUBTRACT,
	NUMPAD_ADD,
	NUMPAD_ENTER,
	NUMPAD_EQUAL,
	LEFT_SHIFT = 340,
	LEFT_CONTROL,
	LEFT_ALT,
	LEFT_SUPER,
	RIGHT_SHIFT,
	RIGHT_CONTROL,
	RIGHT_ALT,
	RIGHT_SUPER,
	MENU,

	UNKNOWN = UINT16_MAX
};

enum class InputState : uint8_t
{
	RELEASE,
	PRESS,
	HOLD,
	MOUSE_AXIS
};

namespace p = std::placeholders;
#define MAKE_CB(func, obj) std::bind(&func, obj)
#define MAKE_CB_1(func, obj) std::bind(&func, obj, p::_1)

namespace InputManagerImpl {
	using EventInputTypes = std::tuple<std::monostate, float, glm::vec2>;

	template<typename T>
	struct InputEvent
	{
		using ValueType = T;
		typedef std::function<void(const ValueType&)> Callback;

		Callback callback;
		ValueType scale;
	};

	template<>
	struct InputEvent<std::monostate>
	{
		using ValueType = void;
		typedef std::function<void()> Callback;

		Callback callback;
	};

	template<typename T>
	struct InputEventVariant;

	template<typename... Ts>
	struct InputEventVariant<std::tuple<Ts...>>
	{
		using type = std::variant<InputEvent<Ts>...>;
	};

	template<typename T>
	struct ModifierVariant;

	template<typename... Ts>
	struct ModifierVariant<std::tuple<Ts...>>
	{
		using type = std::variant<Ts...>;
	};

	using InputEvent_Typed = InputEventVariant<EventInputTypes>::type;
	using Modifier_Typed = ModifierVariant<EventInputTypes>::type;

	template<typename T, typename Ts>
	struct TupleContains;

	template<typename T, typename... Ts>
	struct TupleContains<T, std::tuple<Ts...>>
	{
		static constexpr bool value = (std::is_same_v<T, Ts> || ...);
	};
}

template<typename T>
concept ValidEventInput = InputManagerImpl::TupleContains<T, InputManagerImpl::EventInputTypes>::value;

template<ValidEventInput T>
using EventCallback = InputManagerImpl::InputEvent<T>::Callback;

class InputManager : public ISingleton<InputManager>
{
protected:
	std::unordered_map<Input, InputState> mButtonStates;
	std::unordered_map<size_t, InputManagerImpl::Modifier_Typed> mModifiers;

	std::unordered_map<size_t, std::vector<InputManagerImpl::InputEvent_Typed>> mInputEvents;
	std::vector<size_t> mQueuedEvents;

public:
	void OnInitialized();
	void OnCleanup();

	void BindToWindow(Window& _window);

	template<ValidEventInput T>
	void AddInputEvent(Input _input, InputState _state, const T& _scale, EventCallback<T> _callback) {
		const size_t eventHash = GetInputEventHash(_input, _state);
		mInputEvents[eventHash].push_back(InputManagerImpl::InputEvent<T>{
			.callback = _callback,
			.scale = _scale
		});
	}
	void AddInputEvent(Input _input, InputState _state, EventCallback<std::monostate> _callback);

	void DispatchEvents();

protected:
	constexpr size_t GetInputEventHash(Input _input, InputState _state) const;
	void DeconstructInputEventHash(size_t _hash, Input& _outInput, InputState& _outState);
	constexpr InputState GetStateFromAction(int _action) const;

	bool DispatchInputEvent(size_t _eventHash);

	void ProcessButtonInput(int _button, int _action);
	void ProcessMouseMovement(const glm::vec2& _deltaPos);
	void ProcessMouseScroll(const glm::vec2& _scrollDelta);
};