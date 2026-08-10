#pragma once

#include "../pch.h"

class IWindow
{
public:
	virtual ~IWindow() = default;

	virtual HWND GetNativeHandle() const = 0;
	virtual int GetWidth() const = 0;
	virtual int GetHeight() const = 0;

	virtual bool Create(HINSTANCE hInstance, int width, int height, const wchar_t* title) = 0;
	virtual void Show(int cmdShow) = 0;
	virtual void Minimize() = 0;
	virtual void ToggleMaximizeRestore() = 0;
	virtual void Close() = 0;
	virtual void BeginTitleBarDrag() = 0;
	virtual bool IsMaximized() const = 0;
};
