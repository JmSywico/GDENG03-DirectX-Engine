#include "pch.h"

#include "Engine.h"
#include "Platform/WindowsDpi.h"

#include <shellapi.h>

namespace
{
	std::filesystem::path GetScenePath(const enignE::Project::ProjectConfig& project)
	{
		int argumentCount = 0;
		LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
		std::filesystem::path result = project.ResolveStartupScene();
		if (arguments && argumentCount > 1)
		{
			const std::filesystem::path argument = arguments[1];
			result = argument.is_absolute()
				? argument.lexically_normal()
				: (std::filesystem::current_path() / argument).lexically_normal();
		}
		if (arguments)
			LocalFree(arguments);
		return result;
	}
}

int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE,
	_In_ PWSTR,
	_In_ int)
{
	enignE::Platform::EnablePerMonitorDpiAwareness();
	wchar_t executablePath[MAX_PATH] = {};
	if (GetModuleFileNameW(
			nullptr,
			executablePath,
			static_cast<DWORD>(std::size(executablePath))) != 0)
	{
		std::filesystem::current_path(std::filesystem::path(executablePath).parent_path());
	}

	if (!Logging::Initialize("enignERuntime.log", Logging::Level::Info))
		MessageBox(nullptr, L"File logging could not be initialized.", L"Logging warning", MB_OK);

	Engine app(hInstance, Engine::AppMode::Runtime);
	if (!app.Initialize())
	{
		Logging::Shutdown();
		return -1;
	}

	const std::filesystem::path scenePath = GetScenePath(app.GetProjectConfig());
	if (!app.LoadScene(scenePath))
	{
		LOG_ERRORF("Unable to load packaged runtime scene '{}'", scenePath.string());
		app.Cleanup();
		Logging::Shutdown();
		return -2;
	}

	const int result = app.Run();
	app.Cleanup();
	Logging::Shutdown();
	return result;
}
