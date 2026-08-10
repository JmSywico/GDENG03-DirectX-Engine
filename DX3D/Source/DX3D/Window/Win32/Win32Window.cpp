#include <DX3D/Window/Window.h>

#include <Windows.h>

#include <imgui.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT
ImGui_ImplWin32_WndProcHandler(
	HWND hwnd,
	UINT msg,
	WPARAM wparam,
	LPARAM lparam
);

static LRESULT CALLBACK WindowProcedure(
	HWND hwnd,
	UINT msg,
	WPARAM wparam,
	LPARAM lparam
)
{
	if (
		ImGui::GetCurrentContext() &&
		ImGui_ImplWin32_WndProcHandler(
			hwnd,
			msg,
			wparam,
			lparam
		)
		)
	{
		return 1;
	}

	switch (msg)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;

	default:
		return DefWindowProc(
			hwnd,
			msg,
			wparam,
			lparam
		);
	}
}

dx3d::Window::Window(
	const WindowDesc& desc
)
	: Base(desc.base),
	m_size(desc.size)
{
	auto registerWindowClassFunction =
		[]()
		{
			WNDCLASSEX windowClass{};

			windowClass.cbSize =
				sizeof(WNDCLASSEX);

			windowClass.lpszClassName =
				L"DX3DWindow";

			windowClass.lpfnWndProc =
				&WindowProcedure;

			windowClass.hInstance =
				GetModuleHandle(nullptr);

			windowClass.hCursor =
				LoadCursor(
					nullptr,
					IDC_ARROW
				);

			return RegisterClassEx(
				&windowClass
			);
		};

	static const auto windowClassId =
		registerWindowClassFunction();

	if (!windowClassId)
	{
		DX3DLogThrowError(
			"RegisterClassEx failed."
		);
	}

	constexpr DWORD windowStyle = WS_POPUP;
	const HMONITOR monitor = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO monitorInfo{ sizeof(MONITORINFO) };
	if (!GetMonitorInfo(monitor, &monitorInfo))
		DX3DLogThrowError("GetMonitorInfo failed.");
	const RECT monitorRectangle = monitorInfo.rcMonitor;
	const int monitorWidth = monitorRectangle.right - monitorRectangle.left;
	const int monitorHeight = monitorRectangle.bottom - monitorRectangle.top;
	const int restoreWidth = (std::min)(m_size.width, monitorWidth);
	const int restoreHeight = (std::min)(m_size.height, monitorHeight);
	m_restoreRect = {
		monitorRectangle.left + (monitorWidth - restoreWidth) / 2,
		monitorRectangle.top + (monitorHeight - restoreHeight) / 2,
		restoreWidth,
		restoreHeight
	};
	m_size = { monitorRectangle.left, monitorRectangle.top, monitorWidth, monitorHeight };

	m_handle =
		CreateWindowEx(
			0,
			MAKEINTATOM(windowClassId),
			L"jnpf.",
			windowStyle,
			monitorRectangle.left,
			monitorRectangle.top,
			monitorWidth,
			monitorHeight,
			nullptr,
			nullptr,
			GetModuleHandle(nullptr),
			nullptr
		);

	if (!m_handle)
	{
		DX3DLogThrowError(
			"CreateWindowEx failed."
		);
	}

	ShowWindow(
		static_cast<HWND>(m_handle),
		SW_SHOW
	);
	UpdateWindow(static_cast<HWND>(m_handle));
}

void*
dx3d::Window::getNativeHandle() const noexcept
{
	return m_handle;
}

dx3d::Rect
dx3d::Window::getClientSize() const noexcept
{
	const auto hwnd =
		static_cast<HWND>(m_handle);

	RECT clientRectangle{};

	if (!GetClientRect(
		hwnd,
		&clientRectangle
	))
	{
		return {};
	}

	return
	{
		0,
		0,
		clientRectangle.right -
			clientRectangle.left,
		clientRectangle.bottom -
			clientRectangle.top
	};
}

dx3d::Rect
dx3d::Window::getClientAreaInScreenSpace()
{
	const auto hwnd =
		static_cast<HWND>(m_handle);

	RECT client{};

	GetClientRect(
		hwnd,
		&client
	);

	POINT topLeft
	{
		client.left,
		client.top
	};

	POINT bottomRight
	{
		client.right,
		client.bottom
	};

	ClientToScreen(
		hwnd,
		&topLeft
	);

	ClientToScreen(
		hwnd,
		&bottomRight
	);

	return
	{
		topLeft.x,
		topLeft.y,
		bottomRight.x -
			topLeft.x,
		bottomRight.y -
			topLeft.y
	};
}

void dx3d::Window::minimize()
{
	if (m_handle) ShowWindow(static_cast<HWND>(m_handle), SW_MINIMIZE);
}

void dx3d::Window::toggleMaximizeRestore()
{
	if (!m_handle) return;
	const HWND window = static_cast<HWND>(m_handle);
	if (m_maximized)
	{
		SetWindowPos(window, nullptr, m_restoreRect.left, m_restoreRect.top,
			m_restoreRect.width, m_restoreRect.height,
			SWP_NOZORDER | SWP_FRAMECHANGED);
		m_maximized = false;
		return;
	}
	RECT restore{};
	GetWindowRect(window, &restore);
	m_restoreRect = { restore.left, restore.top,
		restore.right - restore.left, restore.bottom - restore.top };
	const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info{ sizeof(MONITORINFO) };
	if (!GetMonitorInfo(monitor, &info)) return;
	const RECT rectangle = info.rcMonitor;
	SetWindowPos(window, nullptr, rectangle.left, rectangle.top,
		rectangle.right - rectangle.left, rectangle.bottom - rectangle.top,
		SWP_NOZORDER | SWP_FRAMECHANGED);
	m_maximized = true;
}

void dx3d::Window::close()
{
	if (m_handle) PostMessage(static_cast<HWND>(m_handle), WM_CLOSE, 0, 0);
}

void dx3d::Window::beginTitleBarDrag()
{
	if (!m_handle) return;
	if (m_maximized) toggleMaximizeRestore();
	ReleaseCapture();
	SendMessage(static_cast<HWND>(m_handle), WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

bool dx3d::Window::isMaximized() const noexcept
{
	return m_maximized;
}

dx3d::Window::~Window()
{
	DestroyWindow(
		static_cast<HWND>(m_handle)
	);
}
