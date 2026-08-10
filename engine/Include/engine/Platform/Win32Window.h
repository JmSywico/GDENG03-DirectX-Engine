#pragma once

#include "../pch.h"
#include "../Platform/IWindow.h"

class Win32Window : public IWindow
{
public:
	Win32Window();
	~Win32Window() override;

	HWND GetNativeHandle() const override { return m_hWnd; }
	int GetWidth() const override { return m_width; }
	int GetHeight() const override { return m_height; }

	bool Create(HINSTANCE hInstance, int width, int height, const wchar_t* title) override;
	void Show(int cmdShow) override;
	void Minimize() override;
	void ToggleMaximizeRestore() override;
	void Close() override;
	void BeginTitleBarDrag() override;
	bool IsMaximized() const override { return m_maximized; }

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	HINSTANCE m_hInstance;
	HWND m_hWnd;
	int m_width;
	int m_height;
	std::wstring m_title;
	RECT m_restoreRect{};
	bool m_maximized = true;
};
