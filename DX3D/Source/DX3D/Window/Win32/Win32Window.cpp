#include <DX3D/Window/Window.h>
#include <Windows.h>
#include <stdexcept>

static LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_CLOSE:
	{
		PostQuitMessage(0);
		break;
	}
	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}

dx3d::Window::Window(const WindowDesc& desc) : Base(desc.base), m_size(desc.size)
{
	auto registerWindowClassFunction = []()
		{
			WNDCLASSEX wc{};
			wc.cbSize = sizeof(WNDCLASSEX);
			wc.lpszClassName = L"DX3DWindow";
			wc.lpfnWndProc = &WindowProcedure;
			return RegisterClassEx(&wc);
		};


	static const auto windowClassId = std::invoke(registerWindowClassFunction);


	if (!windowClassId)
		DX3DLogThrowError("RegisterClassEx failed.");
	
	RECT rc{ 0,0,m_size.width, m_size.height };
	AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, false);

	m_handle = CreateWindowEx(NULL, MAKEINTATOM(windowClassId), L"GDENG03 | DIRECTX Game Engine",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left, rc.bottom - rc.top,
		NULL, NULL, NULL, NULL);

	if (!m_handle)
		DX3DLogThrowError("CreateWindowEx failed.");

	ShowWindow(static_cast<HWND>(m_handle), SW_SHOW);
}


dx3d::Rect dx3d::Window::getClientAreaInScreenSpace()
{
	auto hwnd = static_cast<HWND>(m_handle);
	
	RECT client{};
	GetClientRect(hwnd, &client);

	POINT topLeft{ client.left, client.top };
	POINT bottomRight{ client.right, client.bottom };
	ClientToScreen(hwnd, &topLeft);
	ClientToScreen(hwnd, &bottomRight);

	return { 
		topLeft.x , 
		topLeft.y , 
		bottomRight.x - topLeft.x, 
		bottomRight.y - topLeft.y 
	};
}




dx3d::Window::~Window()
{
	DestroyWindow(static_cast<HWND>(m_handle));
}
