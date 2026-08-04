#pragma once
#include <stdexcept>
#include <memory>
#include <cstdint>

#define dx3d_disable_copy_and_move(Class)     \
protected:\
    Class(const Class&) = delete;        \
    Class& operator=(const Class&) = delete; \
    Class(Class&&) = delete;             \
    Class& operator=(Class&&) = delete;

namespace dx3d
{
	class Base;
	class Window;
	class Game;
	class InputSystem;
	class GraphicsEngine;
	class GraphicsDevice;
	class Logger;
	class SwapChain;
	class Display;
	class DeviceContext;
	class ShaderBinary;
	class GraphicsPipelineState;
	class VertexBuffer;
	class VertexShaderSignature;
	class ConstantBuffer;
	class IndexBuffer;

	class World;
	class GameObject;
	class Component;
	class TransformComponent;

	class WorldRenderer;


	using i32 = int;
	using ui32 = unsigned int;
	using ui64 = std::uint64_t;
	using f32 = float;
	using d64 = double;

	template <typename T> using RefPtr = std::shared_ptr<T>;
	template <typename T> using UniquePtr = std::unique_ptr<T>;
}
