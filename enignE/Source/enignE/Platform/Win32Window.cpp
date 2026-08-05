#include "Platform/Win32Window.h"

namespace
{
	constexpr wchar_t CLASS_NAME[] = L"enignEWindowClass_Platform";
}

Win32Window::Win32Window()
	: m_hInstance(nullptr)
	, m_hWnd(nullptr)
	, m_width(800)
	, m_height(600)
{
}

Win32Window::~Win32Window()
{
	if (m_hWnd)
	{
		DestroyWindow(m_hWnd);
		m_hWnd = nullptr;
	}
}

bool Win32Window::Create(HINSTANCE hInstance, int width, int height, const wchar_t* title)
{
	m_hInstance = hInstance;
	m_width = width;
	m_height = height;
	m_title = title ? title : L"";

	WNDCLASS wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

	const HMONITOR monitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO monitorInfo{sizeof(MONITORINFO)};
	if (!GetMonitorInfo(monitor, &monitorInfo))
		return false;
	const RECT& monitorRect = monitorInfo.rcMonitor;
	m_width = monitorRect.right - monitorRect.left;
	m_height = monitorRect.bottom - monitorRect.top;
	const int restoreWidth = std::min(width, m_width);
	const int restoreHeight = std::min(height, m_height);
	m_restoreRect = {
		monitorRect.left + (m_width - restoreWidth) / 2,
		monitorRect.top + (m_height - restoreHeight) / 2,
		monitorRect.left + (m_width + restoreWidth) / 2,
		monitorRect.top + (m_height + restoreHeight) / 2
	};

	m_hWnd = CreateWindowEx(
		0,
		CLASS_NAME,
		m_title.c_str(),
		WS_POPUP,
		monitorRect.left, monitorRect.top,
		m_width, m_height,
		nullptr,
		nullptr,
		hInstance,
		this);

	if (!m_hWnd)
		return false;

	return true;
}

void Win32Window::Show(int cmdShow)
{
	if (m_hWnd)
	{
		ShowWindow(m_hWnd, cmdShow);
		UpdateWindow(m_hWnd);
	}
}

void Win32Window::Minimize()
{
	if (m_hWnd)
		ShowWindow(m_hWnd, SW_MINIMIZE);
}

void Win32Window::ToggleMaximizeRestore()
{
	if (!m_hWnd)
		return;

	if (m_maximized)
	{
		SetWindowPos(
			m_hWnd,
			nullptr,
			m_restoreRect.left,
			m_restoreRect.top,
			m_restoreRect.right - m_restoreRect.left,
			m_restoreRect.bottom - m_restoreRect.top,
			SWP_NOZORDER | SWP_FRAMECHANGED);
		m_maximized = false;
	}
	else
	{
		GetWindowRect(m_hWnd, &m_restoreRect);
		const HMONITOR monitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO monitorInfo{sizeof(MONITORINFO)};
		if (GetMonitorInfo(monitor, &monitorInfo))
		{
			const RECT& rect = monitorInfo.rcMonitor;
			SetWindowPos(
				m_hWnd,
				nullptr,
				rect.left,
				rect.top,
				rect.right - rect.left,
				rect.bottom - rect.top,
				SWP_NOZORDER | SWP_FRAMECHANGED);
			m_maximized = true;
		}
	}
}

void Win32Window::Close()
{
	if (m_hWnd)
		PostMessage(m_hWnd, WM_CLOSE, 0, 0);
}

void Win32Window::BeginTitleBarDrag()
{
	if (!m_hWnd)
		return;
	if (m_maximized)
		ToggleMaximizeRestore();
	ReleaseCapture();
	SendMessage(m_hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

LRESULT CALLBACK Win32Window::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	Win32Window* window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	if (message == WM_NCCREATE)
	{
		const auto* create = reinterpret_cast<CREATESTRUCT*>(lParam);
		window = static_cast<Win32Window*>(create->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
	}

	InputManager::Get().HandleMessage(hWnd, message, wParam, lParam);

	switch (message)
	{
	case WM_DPICHANGED:
		if (window)
		{
			const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
			SetWindowPos(hWnd, nullptr, suggested->left, suggested->top,
				suggested->right - suggested->left, suggested->bottom - suggested->top,
				SWP_NOZORDER | SWP_NOACTIVATE);
		}
		return 0;
	case WM_SIZE:
		if (window && wParam != SIZE_MINIMIZED)
		{
			window->m_width = std::max(1, static_cast<int>(LOWORD(lParam)));
			window->m_height = std::max(1, static_cast<int>(HIWORD(lParam)));
		}
		return 0;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}
