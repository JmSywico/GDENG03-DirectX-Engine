#include "Editor/UI/EditorLoadingWindow.h"

#include <algorithm>
#include <chrono>

namespace
{
	constexpr wchar_t WindowClassName[] = L"jnpf.EditorLoadingWindow";

	std::wstring Widen(const char* value)
	{
		if (!value || !*value) return {};
		const int length = MultiByteToWideChar(CP_UTF8, 0, value, -1, nullptr, 0);
		if (length <= 1) return {};
		std::wstring result(static_cast<std::size_t>(length), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, value, -1, result.data(), length);
		result.resize(static_cast<std::size_t>(length - 1));
		return result;
	}

	void Fill(HDC context, const RECT& rectangle, COLORREF color)
	{
		HBRUSH brush = CreateSolidBrush(color);
		FillRect(context, &rectangle, brush);
		DeleteObject(brush);
	}

	void Frame(HDC context, const RECT& rectangle, COLORREF color)
	{
		HBRUSH brush = CreateSolidBrush(color);
		FrameRect(context, &rectangle, brush);
		DeleteObject(brush);
	}
}

namespace jnpf::Editor::UI
{
	EditorLoadingWindow::~EditorLoadingWindow() { Stop(); }

	bool EditorLoadingWindow::Start(const char* title, const char* detail)
	{
		Stop();
		HWND owner = GetActiveWindow();
		if (!owner) owner = GetForegroundWindow();
		{
			std::scoped_lock lock(m_mutex);
			m_ready = false;
			m_created = false;
			m_window = nullptr;
			m_threadID = 0;
		}
		m_thread = std::jthread(
			[this, owner, windowTitle = Widen(title), windowDetail = Widen(detail)](std::stop_token stopToken) mutable
			{
				ThreadMain(stopToken, owner, std::move(windowTitle), std::move(windowDetail));
			});
		std::unique_lock lock(m_mutex);
		m_readyCondition.wait_for(lock, std::chrono::seconds(2), [this] { return m_ready; });
		return m_created;
	}

	void EditorLoadingWindow::Stop()
	{
		if (!m_thread.joinable()) return;
		m_thread.request_stop();
		HWND window = nullptr;
		DWORD threadID = 0;
		{
			std::scoped_lock lock(m_mutex);
			window = m_window;
			threadID = m_threadID;
		}
		if (window) PostMessageW(window, WM_CLOSE, 0, 0);
		else if (threadID != 0) PostThreadMessageW(threadID, WM_QUIT, 0, 0);
		m_thread.join();
	}

	void EditorLoadingWindow::ThreadMain(
		std::stop_token stopToken,
		HWND owner,
		std::wstring title,
		std::wstring detail)
	{
		m_title = std::move(title);
		m_detail = std::move(detail);
		m_startedAt = GetTickCount64();
		{
			std::scoped_lock lock(m_mutex);
			m_threadID = GetCurrentThreadId();
		}

		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = &EditorLoadingWindow::WindowProcedure;
		windowClass.hInstance = GetModuleHandleW(nullptr);
		windowClass.hCursor = LoadCursorW(nullptr, IDC_WAIT);
		windowClass.lpszClassName = WindowClassName;
		RegisterClassExW(&windowClass);

		const UINT dpi = owner ? GetDpiForWindow(owner) : 96u;
		const int width = MulDiv(460, static_cast<int>(dpi), 96);
		const int height = MulDiv(142, static_cast<int>(dpi), 96);
		RECT ownerRect{};
		if (!owner || !GetWindowRect(owner, &ownerRect))
			ownerRect = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
		const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
		const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;

		m_titleFont = CreateFontW(-MulDiv(16, static_cast<int>(dpi), 96), 0, 0, 0, FW_SEMIBOLD,
			FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
		m_detailFont = CreateFontW(-MulDiv(13, static_cast<int>(dpi), 96), 0, 0, 0, FW_NORMAL,
			FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

		HWND window = CreateWindowExW(
			WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, WindowClassName, L"jnpf operation", WS_POPUP,
			x, y, width, height, owner, nullptr, GetModuleHandleW(nullptr), this);
		{
			std::scoped_lock lock(m_mutex);
			m_window = window;
			m_created = window != nullptr;
		}
		if (window)
		{
			ShowWindow(window, SW_SHOWNOACTIVATE);
			SetTimer(window, 1, 16, nullptr);
			RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
		}
		{
			std::scoped_lock lock(m_mutex);
			m_ready = true;
		}
		m_readyCondition.notify_all();

		MSG message{};
		while (window && !stopToken.stop_requested() && GetMessageW(&message, nullptr, 0, 0) > 0)
		{
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		if (window && IsWindow(window)) DestroyWindow(window);
		if (m_titleFont) DeleteObject(m_titleFont);
		if (m_detailFont) DeleteObject(m_detailFont);
		m_titleFont = nullptr;
		m_detailFont = nullptr;
		{
			std::scoped_lock lock(m_mutex);
			m_window = nullptr;
			m_threadID = 0;
		}
	}

	void EditorLoadingWindow::Paint(HWND window)
	{
		PAINTSTRUCT paint{};
		HDC context = BeginPaint(window, &paint);
		RECT bounds{};
		GetClientRect(window, &bounds);
		const int dpi = static_cast<int>(GetDpiForWindow(window));
		const auto scale = [dpi](int value) { return MulDiv(value, dpi, 96); };
		Fill(context, bounds, RGB(24, 24, 28));
		Frame(context, bounds, RGB(76, 72, 64));
		Fill(context, RECT{0, 0, scale(4), bounds.bottom}, RGB(224, 161, 65));

		SetBkMode(context, TRANSPARENT);
		SetTextColor(context, RGB(236, 232, 222));
		SelectObject(context, m_titleFont);
		RECT titleRect{scale(25), scale(20), bounds.right - scale(24), scale(48)};
		DrawTextW(context, m_title.c_str(), -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
		SetTextColor(context, RGB(156, 152, 143));
		SelectObject(context, m_detailFont);
		RECT detailRect{scale(25), scale(53), bounds.right - scale(24), scale(81)};
		DrawTextW(context, m_detail.c_str(), -1, &detailRect,
			DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

		const int railLeft = scale(25);
		const int railRight = bounds.right - scale(24);
		const int railTop = bounds.bottom - scale(27);
		Fill(context, RECT{railLeft, railTop, railRight, railTop + scale(3)}, RGB(54, 53, 51));
		const int travel = std::max(1, railRight - railLeft);
		const int segmentWidth = std::max(scale(18), travel * 24 / 100);
		const double cycle = static_cast<double>((GetTickCount64() - m_startedAt) % 1400u) / 1400.0;
		const int segmentLeft = railLeft + static_cast<int>((travel - segmentWidth) * cycle);
		Fill(context, RECT{segmentLeft, railTop, segmentLeft + segmentWidth, railTop + scale(3)},
			RGB(224, 161, 65));
		EndPaint(window, &paint);
	}

	LRESULT CALLBACK EditorLoadingWindow::WindowProcedure(
		HWND window, UINT message, WPARAM wParam, LPARAM lParam)
	{
		EditorLoadingWindow* self = reinterpret_cast<EditorLoadingWindow*>(
			GetWindowLongPtrW(window, GWLP_USERDATA));
		if (message == WM_NCCREATE)
		{
			const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
			self = static_cast<EditorLoadingWindow*>(create->lpCreateParams);
			SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		}
		switch (message)
		{
		case WM_TIMER: InvalidateRect(window, nullptr, FALSE); return 0;
		case WM_ERASEBKGND: return 1;
		case WM_PAINT: if (self) self->Paint(window); return 0;
		case WM_CLOSE: DestroyWindow(window); return 0;
		case WM_DESTROY: KillTimer(window, 1); PostQuitMessage(0); return 0;
		default: return DefWindowProcW(window, message, wParam, lParam);
		}
	}
}
