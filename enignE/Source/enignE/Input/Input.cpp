#include "Input/Input.h"

InputManager::InputManager()
{
}

void InputManager::Initialize(HWND hwnd, bool enableRaw)
{
	m_hwnd = hwnd;
	m_hasFocus = GetForegroundWindow() == m_hwnd;
	EnableRawInput(enableRaw);
}

void InputManager::EnableRawInput(bool enable)
{
	if (enable == m_rawInputEnabled)
		return;

	RegisterRawMouse(enable);
	m_rawInputEnabled = enable;
}

void InputManager::RegisterRawMouse(bool enable)
{
	if (!m_hwnd)
		return;

	if (!enable)
	{
		RAWINPUTDEVICE ridRemove{};
		ridRemove.usUsagePage = 0x01;
		ridRemove.usUsage = 0x02;
		ridRemove.dwFlags = RIDEV_REMOVE;
		ridRemove.hwndTarget = nullptr;
		RegisterRawInputDevices(&ridRemove, 1, sizeof(ridRemove));
		return;
	}

	RAWINPUTDEVICE rid{};
	rid.usUsagePage = 0x01;
	rid.usUsage = 0x02;
	rid.dwFlags = 0;
	rid.hwndTarget = m_hwnd;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

void InputManager::SetFocusState(bool hasFocus)
{
	if (m_hasFocus == hasFocus)
		return;

	m_hasFocus = hasFocus;
	if (!m_hasFocus)
	{
		CaptureMouse(false);
		ResetInputState();
	}
}

void InputManager::ResetInputState()
{
	m_currentKeyState.fill(false);
	m_previousKeyState.fill(false);
	m_keyMessageFlag.fill(false);
	m_currentMouseButtons.fill(false);
	m_inputCharacter = -1;
}

void InputManager::CaptureMouse(bool capture)
{
	if (capture == m_mouseCaptured)
		return;

	m_mouseCaptured = capture;
	if (capture && m_hwnd)
	{
		m_hasMousePositionBeforeCapture = GetCursorPos(&m_mousePositionBeforeCapture) != FALSE;
		RECT r;
		if (GetClientRect(m_hwnd, &r))
		{
			POINT ul{r.left, r.top};
			POINT lr{r.right, r.bottom};
			ClientToScreen(m_hwnd, &ul);
			ClientToScreen(m_hwnd, &lr);
			RECT clip{ul.x, ul.y, lr.x, lr.y};
			ClipCursor(&clip);
			SetCapture(m_hwnd);
			ShowCursor(FALSE);
		}
	}
	else
	{
		ClipCursor(nullptr);
		ReleaseCapture();
		ShowCursor(TRUE);
		if (m_hasFocus && m_hasMousePositionBeforeCapture)
		{
			SetCursorPos(m_mousePositionBeforeCapture.x, m_mousePositionBeforeCapture.y);
			m_mousePos = m_mousePositionBeforeCapture;
			if (m_hwnd)
				ScreenToClient(m_hwnd, &m_mousePos);
		}
		m_hasMousePositionBeforeCapture = false;
	}
}

// Raw input and borderless windows can leave resize cursors active over the client area.
static void ApplyCorrectCursor(HWND hwnd, bool mouseCaptured, const POINT* clientPt = nullptr)
{
	if (mouseCaptured)
	{
		SetCursor(nullptr);
		return;
	}

	if (hwnd && clientPt)
	{
		RECT rc;
		if (GetClientRect(hwnd, &rc))
		{
			if (clientPt->x >= rc.left && clientPt->x < rc.right &&
				clientPt->y >= rc.top && clientPt->y < rc.bottom)
			{
				SetCursor(LoadCursor(nullptr, IDC_ARROW));
			}
		}
	}
}

bool InputManager::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (!m_hwnd && hWnd)
		m_hwnd = hWnd;

	switch (message)
	{
	case WM_ACTIVATE:
		SetFocusState(LOWORD(wParam) != WA_INACTIVE);
		break;
	case WM_SETFOCUS:
		SetFocusState(true);
		break;
	case WM_KILLFOCUS:
		SetFocusState(false);
		break;
	case WM_INPUT:
		{
			if (!m_rawInputEnabled || !m_hasFocus || GetForegroundWindow() != m_hwnd)
				break;

			UINT dwSize = 0;
			GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
			if (dwSize == 0)
				break;

			std::vector<uint8_t> buffer(dwSize);
			if (GetRawInputData(
					reinterpret_cast<HRAWINPUT>(lParam),
					RID_INPUT,
					buffer.data(),
					&dwSize,
					sizeof(RAWINPUTHEADER)) != dwSize)
				break;

			auto raw = reinterpret_cast<RAWINPUT*>(buffer.data());
			if (raw->header.dwType == RIM_TYPEMOUSE)
			{
				const RAWMOUSE& rm = raw->data.mouse;

				LONG dx = 0;
				LONG dy = 0;
				if (rm.usFlags == MOUSE_MOVE_RELATIVE || (rm.usFlags & MOUSE_MOVE_RELATIVE))
				{
					dx = rm.lLastX;
					dy = rm.lLastY;
				}

				POINT cur{};
				if (GetCursorPos(&cur) && m_hwnd)
				{
					ScreenToClient(m_hwnd, &cur);
					m_mousePos = cur;

					ApplyCorrectCursor(m_hwnd, m_mouseCaptured, &cur);
				}

				if (dx != 0 || dy != 0)
				{
					auto evt = std::make_unique<MouseMoveEvent>(static_cast<int>(dx), static_cast<int>(dy));
					EventManager::Get().QueueEvent(std::move(evt));
				}

				if (rm.usButtonFlags != 0)
				{
					if (rm.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
					{
						m_currentMouseButtons[1] = true;
						auto e = std::make_unique<MouseButtonEvent>(EventType::MouseButtonDown, 1);
						EventManager::Get().QueueEvent(std::move(e));
					}
					if (rm.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
					{
						m_currentMouseButtons[1] = false;
						auto e = std::make_unique<MouseButtonEvent>(EventType::MouseButtonUp, 1);
						EventManager::Get().QueueEvent(std::move(e));
					}
					if (rm.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
					{
						m_currentMouseButtons[2] = true;
						auto e = std::make_unique<MouseButtonEvent>(EventType::MouseButtonDown, 2);
						EventManager::Get().QueueEvent(std::move(e));
					}
					if (rm.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
					{
						m_currentMouseButtons[2] = false;
						auto e = std::make_unique<MouseButtonEvent>(EventType::MouseButtonUp, 2);
						EventManager::Get().QueueEvent(std::move(e));
					}
					if (rm.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
					{
						m_currentMouseButtons[3] = true;
						auto e = std::make_unique<MouseButtonEvent>(EventType::MouseButtonDown, 3);
						EventManager::Get().QueueEvent(std::move(e));
					}
					if (rm.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
					{
						m_currentMouseButtons[3] = false;
						auto e = std::make_unique<MouseButtonEvent>(EventType::MouseButtonUp, 3);
						EventManager::Get().QueueEvent(std::move(e));
					}

					if (rm.usButtonFlags & RI_MOUSE_WHEEL)
					{
						short wheelDelta = static_cast<short>(rm.usButtonData);
						m_mouseWheelPosition += wheelDelta / WHEEL_DELTA;
						auto e = std::make_unique<MouseWheelEvent>(static_cast<int>(wheelDelta), false);
						EventManager::Get().QueueEvent(std::move(e));
					}
					if (rm.usButtonFlags & RI_MOUSE_HWHEEL)
					{
						short wheelDelta = static_cast<short>(rm.usButtonData);
						auto e = std::make_unique<MouseWheelEvent>(static_cast<int>(wheelDelta), true);
						EventManager::Get().QueueEvent(std::move(e));
					}
				}
			}
			return false;
		}
	case WM_CHAR:
		if (m_hasFocus)
			m_inputCharacter = static_cast<int>(wParam);
		return false;
	case WM_SETCURSOR:
		{
			WORD hitTest = LOWORD(lParam);
			if (m_hwnd)
			{
				if (m_mouseCaptured)
				{
					SetCursor(nullptr);
					return true;
				}

				if (hitTest == HTCLIENT)
				{
					SetCursor(LoadCursor(nullptr, IDC_ARROW));
					return true;
				}
			}
			break;
		}
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		{
			if (!m_hasFocus)
				break;

			int key = static_cast<int>(wParam);
			bool repeat = (lParam & (1 << 30)) != 0;

			if (key >= 0 && key < 256)
			{
				m_currentKeyState[key] = true;
				m_keyMessageFlag[key] = true;
			}

			auto evt = std::make_unique<KeyEvent>(EventType::KeyDown, key, repeat);
			EventManager::Get().QueueEvent(std::move(evt));
			return false;
		}
	case WM_KEYUP:
	case WM_SYSKEYUP:
		{
			if (!m_hasFocus)
				break;

			int key = static_cast<int>(wParam);
			if (key >= 0 && key < 256)
			{
				m_currentKeyState[key] = false;
				m_keyMessageFlag[key] = true;
			}

			auto evt = std::make_unique<KeyEvent>(EventType::KeyUp, key, false);
			EventManager::Get().QueueEvent(std::move(evt));
			return false;
		}
	default:
		break;
	}

	return false;
}

void InputManager::PollKeyboard()
{
	if (!m_hasFocus || GetForegroundWindow() != m_hwnd)
	{
		SetFocusState(false);
		return;
	}

	m_previousKeyState = m_currentKeyState;

	for (int vk = 0; vk < 256; ++vk)
	{
		SHORT state = GetAsyncKeyState(vk);
		bool down = (state & 0x8000) != 0;

		if (down != m_currentKeyState[vk])
		{
			if (m_keyMessageFlag[vk])
			{
				m_currentKeyState[vk] = down;
			}
			else
			{
				if (down)
				{
					auto evt = std::make_unique<KeyEvent>(EventType::KeyDown, vk, false);
					EventManager::Get().QueueEvent(std::move(evt));
				}
				else
				{
					auto evt = std::make_unique<KeyEvent>(EventType::KeyUp, vk, false);
					EventManager::Get().QueueEvent(std::move(evt));
				}
				m_currentKeyState[vk] = down;
			}
		}
		else
		{
			m_currentKeyState[vk] = down;
		}
	}

	m_keyMessageFlag.fill(false);
}
