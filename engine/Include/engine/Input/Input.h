#pragma once

#include "../pch.h"

struct KeyEvent : Event
{
	KeyEvent(EventType t, int key, bool repeat)
		: Event(t), Key(key), Repeat(repeat)
	{
	}

	int Key;
	bool Repeat;
};

// Raw-input mouse moves report deltas. Query InputManager for absolute client coordinates.
struct MouseMoveEvent : Event
{
	MouseMoveEvent(int x, int y)
		: Event(EventType::MouseMove), X(x), Y(y)
	{
	}

	int X, Y;
};

struct MouseButtonEvent : Event
{
	MouseButtonEvent(EventType t, int button)
		: Event(t), Button(button)
	{
	}

	int Button;
};

struct MouseWheelEvent : Event
{
	MouseWheelEvent(int delta, bool horizontal = false)
		: Event(EventType::MouseWheel), Delta(delta), Horizontal(horizontal)
	{
	}

	int Delta;
	bool Horizontal;
};

// Combines Win32 message input with per-frame keyboard polling.
class InputManager
{
public:
	static InputManager& Get()
	{
		static InputManager s;
		return s;
	}

	void Initialize(HWND hwnd, bool enableRaw = true);

	// Called by WndProc; returns true only when the message should stop propagating.
	bool HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	void PollKeyboard();

	bool IsKeyDown(int vk) const
	{
		return m_hasFocus && (vk >= 0 && vk < 256) ? m_currentKeyState[vk] : false;
	}

	bool IsMouseButtonDown(int button) const
	{
		return m_hasFocus && (button >= 0 && button < static_cast<int>(m_currentMouseButtons.size()))
			? m_currentMouseButtons[button]
			: false;
	}

	POINT GetMousePosition() const { return m_mousePos; }
	bool HasFocus() const { return m_hasFocus; }
	bool IsMouseCaptured() const { return m_mouseCaptured; }
	int GetMouseWheelPosition() const { return m_mouseWheelPosition; }
	int ConsumeInputCharacter()
	{
		const int character = m_inputCharacter;
		m_inputCharacter = -1;
		return character;
	}

	void EnableRawInput(bool enable);
	void CaptureMouse(bool capture);

private:
	InputManager();

	void RegisterRawMouse(bool enable);
	void SetFocusState(bool hasFocus);
	void ResetInputState();

	HWND m_hwnd{nullptr};
	bool m_rawInputEnabled{false};
	bool m_mouseCaptured{false};
	bool m_hasFocus{false};

	std::array<bool, 256> m_currentKeyState{};
	std::array<bool, 256> m_previousKeyState{};
	std::array<bool, 256> m_keyMessageFlag{};

	// Index 0 is unused; 1, 2, and 3 map to left, right, and middle buttons.
	std::array<bool, 5> m_currentMouseButtons{};

	POINT m_mousePos{0, 0};
	POINT m_mousePositionBeforeCapture{0, 0};
	bool m_hasMousePositionBeforeCapture{false};
	int m_mouseWheelPosition{0};
	int m_inputCharacter{-1};
};
