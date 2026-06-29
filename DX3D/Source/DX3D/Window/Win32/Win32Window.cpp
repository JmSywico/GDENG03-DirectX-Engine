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

	constexpr DWORD windowStyle =
		WS_OVERLAPPEDWINDOW;

	RECT windowRectangle
	{
		0,
		0,
		m_size.width,
		m_size.height
	};

	AdjustWindowRect(
		&windowRectangle,
		windowStyle,
		FALSE
	);

	m_handle =
		CreateWindowEx(
			0,
			MAKEINTATOM(windowClassId),
			L"GDENG03 | DIRECTX Game Engine",
			windowStyle,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			windowRectangle.right -
			windowRectangle.left,
			windowRectangle.bottom -
			windowRectangle.top,
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

dx3d::Window::~Window()
{
	DestroyWindow(
		static_cast<HWND>(m_handle)
	);
}
