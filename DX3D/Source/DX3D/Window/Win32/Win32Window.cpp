#include <DX3D/Window/Window.h>

#include <Windows.h>
#include <Shellapi.h>

#include <mutex>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_impl_win32.h>

namespace
{
	std::mutex g_droppedFilesMutex{};
	std::vector<std::string> g_droppedFiles{};

	std::string toUtf8(
		const std::wstring& text
	)
	{
		if (text.empty())
			return {};

		const int length =
			WideCharToMultiByte(
				CP_UTF8,
				0,
				text.c_str(),
				static_cast<int>(text.size()),
				nullptr,
				0,
				nullptr,
				nullptr
			);

		if (length <= 0)
			return {};

		std::string result(
			static_cast<size_t>(length),
			'\0'
		);

		WideCharToMultiByte(
			CP_UTF8,
			0,
			text.c_str(),
			static_cast<int>(text.size()),
			result.data(),
			length,
			nullptr,
			nullptr
		);

		return result;
	}
}

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
	case WM_DROPFILES:
	{
		const auto dropHandle =
			reinterpret_cast<HDROP>(wparam);

		const UINT fileCount =
			DragQueryFileW(
				dropHandle,
				0xFFFFFFFF,
				nullptr,
				0
			);

		std::vector<std::string> droppedFiles{};

		droppedFiles.reserve(
			fileCount
		);

		for (UINT index = 0;
			index < fileCount;
			++index)
		{
			const UINT pathLength =
				DragQueryFileW(
					dropHandle,
					index,
					nullptr,
					0
				);

			if (pathLength == 0)
				continue;

			std::wstring path(
				static_cast<size_t>(pathLength) + 1u,
				L'\0'
			);

			DragQueryFileW(
				dropHandle,
				index,
				path.data(),
				pathLength + 1
			);

			path.resize(
				pathLength
			);

			droppedFiles.push_back(
				toUtf8(path)
			);
		}

		DragFinish(
			dropHandle
		);

		if (!droppedFiles.empty())
		{
			std::lock_guard lock(
				g_droppedFilesMutex
			);

			g_droppedFiles.insert(
				g_droppedFiles.end(),
				droppedFiles.begin(),
				droppedFiles.end()
			);
		}

		return 0;
	}

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

	DragAcceptFiles(
		static_cast<HWND>(m_handle),
		TRUE
	);

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

std::vector<std::string>
dx3d::Window::consumeDroppedFiles()
{
	std::lock_guard lock(
		g_droppedFilesMutex
	);

	auto droppedFiles =
		std::move(g_droppedFiles);

	g_droppedFiles.clear();

	return droppedFiles;
}

dx3d::Window::~Window()
{
	DragAcceptFiles(
		static_cast<HWND>(m_handle),
		FALSE
	);

	DestroyWindow(
		static_cast<HWND>(m_handle)
	);
}
