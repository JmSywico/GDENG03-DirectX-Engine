#include <DX3D/Game/Game.h>

#include <Windows.h>
#include <mmsystem.h>

#include <thread>
#include <string>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "winmm.lib")

void dx3d::Game::run()
{
	onCreate();

	MSG msg{};

	constexpr auto targetFrameTime =
		std::chrono::nanoseconds(16'666'667);

	timeBeginPeriod(1);

	m_previousTime = std::chrono::steady_clock::now();

	while (m_isRunning)
	{
		const auto frameStart =
			std::chrono::steady_clock::now();

		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				m_isRunning = false;
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (!m_isRunning)
			break;

		onInternalUpdate();

		if (!m_isRunning)
			break;

		const auto targetFrameEnd =
			frameStart + targetFrameTime;


		while (std::chrono::steady_clock::now() < targetFrameEnd)
		{
			const auto remaining =
				targetFrameEnd -
				std::chrono::steady_clock::now();

			if (remaining > std::chrono::milliseconds(2))
			{
				std::this_thread::sleep_for(
					std::chrono::milliseconds(1)
				);
			}
			else
			{
				std::this_thread::yield();
			}
		}

		const auto frameEnd =
			std::chrono::steady_clock::now();

		const std::chrono::duration<double> frameDuration =
			frameEnd - frameStart;

		const double fps =
			frameDuration.count() > 0.0
			? 1.0 / frameDuration.count()
			: 0.0;

		std::ostringstream title;

		title
			<< "Bouncing Circles | FPS: "
			<< std::fixed
			<< std::setprecision(1)
			<< fps;

		const HWND windowHandle = GetActiveWindow();

		if (windowHandle)
		{
			SetWindowTextA(
				windowHandle,
				title.str().c_str()
			);
		}
	}

	timeEndPeriod(1);
}