#include "pch.h"

#include "Engine.h"
#include "Scene/SceneFactory.h"
#include "Platform/WindowsDpi.h"

#include <shellapi.h>

namespace
{
	struct LaunchOptions
	{
		Engine::AppMode Mode = Engine::AppMode::Editor;
		std::filesystem::path ScenePath;
	};

	LaunchOptions ParseLaunchOptions()
	{
		LaunchOptions options;
		int argumentCount = 0;
		LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
		if (!arguments)
			return options;
		for (int index = 1; index < argumentCount; ++index)
		{
			const std::wstring argument = arguments[index];
			if (argument == L"--editor")
			{
				options.Mode = Engine::AppMode::Editor;
			}
			else if (argument == L"--runtime")
			{
				options.Mode = Engine::AppMode::Runtime;
				if (index + 1 < argumentCount)
					options.ScenePath = arguments[++index];
			}
		}
		LocalFree(arguments);
		return options;
	}
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(pCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);
	enignE::Platform::EnablePerMonitorDpiAwareness();

	if (!Logging::Initialize("enignE.log", Logging::Level::Debug))
		MessageBox(nullptr, L"File logging could not be initialized.", L"Logging warning", MB_OK | MB_ICONWARNING);
	LOG_INFO("Application starting");

	const LaunchOptions options = ParseLaunchOptions();
	Engine app(hInstance, options.Mode);

	if (!app.Initialize())
	{
		LOG_ERROR("Failed to initialize engine");
		MessageBox(nullptr, L"Failed to initialize engine.", L"Error", MB_OK | MB_ICONERROR);
		Logging::Shutdown();
		return -1;
	}

	DirectX::XMFLOAT3 eye = {5.0f, 4.5f, -7.5f};
	DirectX::XMFLOAT3 at = {-0.5f, 1.4f, 0.0f};
	DirectX::XMFLOAT3 up = {0.0f, 1.0f, 0.0f};
	app.SetView(eye, at, up);
	float aspect = app.GetViewportAspectRatio();
	app.SetProjection(DirectX::XM_PIDIV4, aspect, 0.1f, 100.0f);
	app.EnableSceneCameraControls(options.Mode == Engine::AppMode::Editor);
	app.SetSceneCameraMoveSpeed(10.0f);

	auto& scene = app.GetScene();
	if (options.Mode == Engine::AppMode::Runtime)
	{
		if (options.ScenePath.empty() || !app.LoadScene(options.ScenePath))
		{
			LOG_ERROR("Runtime mode requires a loadable scene: --runtime <scene-path>");
			app.Cleanup();
			Logging::Shutdown();
			return -2;
		}
	}

	if (options.Mode == Engine::AppMode::Editor)
	{
		enignE::Scene::CreateCamera(
			scene,
			{
				.Name = "Main Camera",
				.Position = {5.0f, 4.5f, -7.5f},
				.Rotation = {0.322f, -0.633f, 0.0f},
				.FOVDegrees = 45.0f,
				.NearPlane = 0.1f,
				.FarPlane = 1000.0f,
				.SetActive = true
			});


		const entt::entity sun = scene.CreateEntity("Sun");
		auto& sunTransform = scene.AddComponent<enignE::Scene::TransformComponent>(sun);
		sunTransform.SetLocalRotation({1.035f, -0.884f, 0.0f});
		scene.AddComponent<enignE::Scene::HierarchyComponent>(sun);
		auto& sunLight = scene.AddComponent<enignE::Scene::LightComponent>(sun);
		sunLight.Color = {1.0f, 0.98f, 0.94f};
		sunLight.Intensity = 1.05f;
		sunLight.ShadowDistance = 100.0f;
		scene.SetActiveLightEntityID(scene.GetEntityID(sun));
	}

	app.SetDirectionalLight(
		DirectX::XMFLOAT3{-0.39f, -0.86f, 0.32f},
		DirectX::XMFLOAT3{1.0f, 0.98f, 0.94f},
		1.05f);

	int result = app.Run();
	LOG_INFO("Application exiting");
	app.Cleanup();
	Logging::Shutdown();

	return result;
}

int main()
{
	return wWinMain(GetModuleHandle(nullptr), nullptr, GetCommandLineW(), SW_SHOWDEFAULT);
}

