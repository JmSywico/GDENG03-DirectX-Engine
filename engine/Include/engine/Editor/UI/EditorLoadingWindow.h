#pragma once

#include <windows.h>

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace jnpf::Editor::UI
{
	/** Responsive native progress window for blocking editor operations. */
	class EditorLoadingWindow
	{
	public:
		EditorLoadingWindow() = default;
		~EditorLoadingWindow();

		EditorLoadingWindow(const EditorLoadingWindow&) = delete;
		EditorLoadingWindow& operator=(const EditorLoadingWindow&) = delete;

		bool Start(const char* title, const char* detail);
		void Stop();

	private:
		void ThreadMain(std::stop_token stopToken, HWND owner, std::wstring title, std::wstring detail);
		void Paint(HWND window);
		static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

		std::jthread m_thread;
		std::mutex m_mutex;
		std::condition_variable m_readyCondition;
		HWND m_window = nullptr;
		DWORD m_threadID = 0;
		bool m_ready = false;
		bool m_created = false;
		std::wstring m_title;
		std::wstring m_detail;
		HFONT m_titleFont = nullptr;
		HFONT m_detailFont = nullptr;
		ULONGLONG m_startedAt = 0;
	};
}
