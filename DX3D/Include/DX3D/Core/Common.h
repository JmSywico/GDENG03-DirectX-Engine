#pragma once
#include <DX3D/Core/Core.h>
#include <DX3D/Core/Logger.h>
#include <DX3D/Math/Rect.h>

namespace dx3d
{
	struct BaseDesc
	{
		Logger& logger;
	};

	struct WindowDesc
	{
		BaseDesc base;
		Rect size{};
	};

	struct DisplayDesc
	{
		WindowDesc window;
		GraphicsDevice& graphicsDevice;
	};

	struct GraphicsDeviceDesc
	{
		BaseDesc base;
	};

	struct SwapChainDesc
	{
		void* winHandle{};
		Rect winSize{};
	};

	enum class ShaderType
	{
		VertexShader = 0,
		PixelShader
	};

	struct ShaderCompileDesc
	{
		const char* shaderSourceName{};
		const void* shaderSourceCode{};
		size_t shaderSourceCodeSize{};
		const char* shaderEntryPoint{};
		ShaderType shaderType{};
	};

	struct VertexShaderSignatureDesc
	{
		const RefPtr<ShaderBinary>& vsBinary;
	};

	struct BinaryData
	{
		const void* data{};
		size_t dataSize{};
	};

	struct GraphicsPipelineStateDesc
	{
		const VertexShaderSignature& vs;
		const ShaderBinary& ps;
	};

	struct VertexBufferDesc
	{
		const void* vertexList{};
		ui32 vertexListSize{};
		ui32 vertexSize{};
	};

	struct ConstantBufferDesc
	{
		const void* buffer{};
		ui32 bufferSize{};
	};

	struct IndexBufferDesc
	{
		const ui32* indexList{};
		ui32 indexListSize{};
	};

	struct ShadowMapDesc
	{
		ui32 width{ 2048 };
		ui32 height{ 2048 };
	};

	struct GameContext
	{
		InputSystem& input;
	};

	struct GameDesc
	{
		Rect windowSize{ 1280,720 };
		Logger::LogLevel logLevel = Logger::LogLevel::Error;
	};

	struct WorldDesc 
	{
		BaseDesc base;
		GameContext gameContext;
	};

	struct GameObjectDesc
	{
		BaseDesc base;
		GameContext gameContext;
		World& world;
	};

	struct ComponentDesc
	{
		BaseDesc base;
		GameObject& object;
		World& world;
	};

	struct WorldRendererDesc
	{
		BaseDesc base;
		GraphicsDevice& engine;
	};

	enum class KeyCode
	{
		Unknown = 0,

		A, B, C, D, E, F, G,
		H, I, J, K, L, M, N,
		O, P, Q, R, S, T, U,
		V, W, X, Y, Z,


		Num0,
		Num1,
		Num2,
		Num3,
		Num4,
		Num5,
		Num6,
		Num7,
		Num8,
		Num9,


		Escape,
		Shift,
		Space,
		Enter,
		Backspace,
		Delete,


		MouseLeft,
		MouseRight,
		MouseMiddle,


		Up,
		Down,
		Left,
		Right,

		Count
	};

	struct InputSystemDesc
	{
		BaseDesc base;
	};

}